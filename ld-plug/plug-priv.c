#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sendfile.h>
#include <sys/mman.h>

#include <elf.h>

#include "confdef.h"

/* Some versions of the elf headers define it as signed int.  */
#undef SHF_EXCLUDE
#define SHF_EXCLUDE	     (1U << 31)

#define HAVE_STDINT_H
#include <plugin-api.h>
_Static_assert(LD_PLUGIN_API_VERSION == 1, "unexpected plugin API version");

#ifndef PLUG_TARGET_ELFCLASS
#if UINTPTR_MAX == UINT64_MAX
#define PLUG_TARGET_ELFCLASS 64
#elif UINTPTR_MAX == UINT32_MAX
#define PLUG_TARGET_ELFCLASS 32
#endif
#endif

#if PLUG_TARGET_ELFCLASS == 64
#define ElfNN_(t) Elf64_##t
#define ELFCLASS ELFCLASS64
#define ELF_ST_BIND(st_info)        ELF64_ST_BIND(st_info)
#define ELF_ST_VISIBILITY(st_other) ELF64_ST_VISIBILITY(st_other)
#define ELF_R_SYM(r_info)           ELF64_R_SYM(r_info)
#elif PLUG_TARGET_ELFCLASS == 32
#define ElfNN_(t) Elf32_##t
#define ELFCLASS ELFCLASS32
#define ELF_ST_BIND(st_info)        ELF32_ST_BIND(st_info)
#define ELF_ST_VISIBILITY(st_other) ELF32_ST_VISIBILITY(st_other)
#define ELF_R_SYM(r_info)           ELF32_R_SYM(r_info)
#endif

#define ELFDATA ELFDATA2LSB

#define PLUG_SECTION_PREFIX ".comment.privplugid."

struct obj {
	char filename[18];
	struct obj *next;
} *objs;

struct sym {
	const char *name;
	enum {V_LOCALIZE, V_HIDE} vis;
};

static struct symsht {
	unsigned *hashes;
	struct sym **syms;
	size_t size;
} syms_htab;

static inline unsigned
sym_hash(const char *name)
{
	const unsigned char *c = (const void *)name;
	unsigned h = 5381;
	for (; *c; c++)
		h += h*32 + *c;
	return h;
}
static struct sym **
sym_htab_lookup(const char *name)
{
	unsigned hash = sym_hash(name);
	for (size_t i = hash; ; i++) {
		i &= syms_htab.size - 1;
		if (!syms_htab.syms[i]) {
			syms_htab.hashes[i] = hash;
			return syms_htab.syms + i;
		}
		if (syms_htab.hashes[i] == hash
		    && !strcmp(name, syms_htab.syms[i]->name))
			return syms_htab.syms + i;
	}
}
static struct sym *
sym_htab_lookup_only(const char *name)
{
	unsigned hash = sym_hash(name);
	for (size_t i = hash; ; i++) {
		i &= syms_htab.size - 1;
		if (!syms_htab.syms[i])
			return 0;
		if (syms_htab.hashes[i] == hash
		    && !strcmp(name, syms_htab.syms[i]->name))
			return syms_htab.syms[i];
	}
}
static const char *
read_syms(const char *file)
{
	FILE *f = fopen(file, "r");
	if (!f)
		return "error opening file";
	int nlocsyms, nhidsyms, sz;
	fscanf(f, "%d %d", &nlocsyms, &nhidsyms);
	for (sz = nlocsyms + nhidsyms; sz & (sz-1); sz &= sz-1);
	sz += 3 * sz;
	syms_htab.hashes = calloc(sz, sizeof *syms_htab.hashes);
	syms_htab.syms = calloc(sz, sizeof *syms_htab.syms);
	syms_htab.size = sz;
	for (int i = 0; i < nlocsyms + nhidsyms; i++) {
		const char *sym;
		fscanf(f, " %ms", &sym);
		struct sym **symp = sym_htab_lookup(sym);
		if (*symp)
			return "duplicate symbol";
		*symp = malloc(sizeof **symp);
		symp[0]->name = sym;
		symp[0]->vis = i < nlocsyms ? V_LOCALIZE : V_HIDE;
	}
	fclose(f);
	return 0;
}
static int
filedup(int fd, off_t offset, off_t filesize)
{
	struct obj *obj = malloc(sizeof *obj);
	strcpy(obj->filename, "/tmp/ldplugXXXXXX");
	obj->next = objs; objs = obj;
	static int tmpfd = -1;
	if (tmpfd >= 0) close(tmpfd);
	tmpfd = mkstemp(obj->filename);
	if (tmpfd >= 0) sendfile(tmpfd, fd, &offset, filesize);
	return tmpfd;
}
static void *
memdup(const void *p, size_t l)
{
	void *r = malloc(l);
	return r ? memcpy(r, p, l) : r;
}
static void *
addrshift(void *addr, unsigned char *cloneview, const unsigned char *view)
{
	return cloneview + ((const unsigned char *)addr - view);
}
static int
ldplug_weak(int elfbind, int elfscn)
{
	if (elfscn == SHN_UNDEF)
		return elfbind == STB_WEAK ? LDPK_WEAKUNDEF : LDPK_UNDEF;
	else if (elfscn == SHN_COMMON)
		return LDPK_COMMON;
	if (elfbind == STB_GNU_UNIQUE) elfbind = STB_WEAK;
	return elfbind == STB_WEAK ? LDPK_WEAKDEF : LDPK_DEF;
}
static int
ldplug_vis(int elfvis)
{
	return (int[]){LDPV_DEFAULT, LDPV_INTERNAL, LDPV_HIDDEN, LDPV_PROTECTED}[elfvis];
}
static const char *
process_elf(int fd, off_t offset, off_t filesize, const unsigned char *view,
	    int *nplugsyms, struct ld_plugin_symbol **plugsyms, int *claim)
{
	const ElfNN_(Ehdr) *ehdr = (void *)view;
	typedef ElfNN_(Shdr) Shdr;
	typedef ElfNN_(Sym) Sym;
	if (memcmp (ehdr->e_ident, ELFMAG, SELFMAG))
		return 0;
	if (ehdr->e_ident[4] != ELFCLASS)
		return "wrong elfclass";
	if (ehdr->e_ident[5] != ELFDATA)
		return "wrong endianness";
	if (ehdr->e_type != ET_REL)
		return 0;
	Shdr *shdrs = (void *)(view + ehdr->e_shoff);
	if (ehdr->e_shentsize != sizeof *shdrs)
		return "Shdr size mismatch";
	int shnum = ehdr->e_shnum;
	if (!shnum && ehdr->e_shoff)
		shnum = shdrs[0].sh_size;
	if (shnum < 2) return 0;
	int shstrndx = ehdr->e_shstrndx;
	if (shstrndx == SHN_XINDEX)
		shstrndx = shdrs[0].sh_link;
	int clonefd = -1;
	unsigned char *cloneview;
	for (int i = 1; i < shnum; i++) {
		Shdr *shdr = shdrs + i;
		if (shdr->sh_type != SHT_SYMTAB) continue;
		Shdr *strtab = shdrs + shdr->sh_link;
		Sym *syms = (void *)(view + shdr->sh_offset);
		char *strings = (void *)(view + strtab->sh_offset);
		strings = memdup(strings, strtab->sh_size);
		size_t nsyms = shdr->sh_size / sizeof (Sym);
		*plugsyms = calloc(nsyms, sizeof **plugsyms);
		for (size_t j = 0; j < nsyms; j++) {
			Sym *sym = syms + j;
			if (!sym->st_name) continue;
			int shndx = sym->st_shndx;
			int bind = ELF_ST_BIND(sym->st_info);
			int vis = ELF_ST_VISIBILITY(sym->st_other);
			char *name = strings + sym->st_name;
			struct sym *s = sym_htab_lookup_only(name);
			if (bind != STB_LOCAL /*&& !strchr(name, '@')*/) {
				struct ld_plugin_symbol *plugsym = *plugsyms + nplugsyms[0]++;
				plugsym->name = name;
				plugsym->def = ldplug_weak(bind, shndx);
				plugsym->visibility
				    = s ? LDPV_HIDDEN : ldplug_vis(vis);
				plugsym->size = sym->st_size;
			}
			if (!s || shndx == SHN_UNDEF) continue;
			if (clonefd < 0) {
				clonefd = filedup(fd, offset, filesize);
				if (clonefd < 0)
					return strerror(errno);
				cloneview = mmap(0, filesize,
						 PROT_READ | PROT_WRITE,
						 MAP_SHARED, clonefd, 0);
			}
			Sym *csym = (void *)addrshift(sym, cloneview, view);
			csym->st_other = STV_HIDDEN;
		}
		break;
	}
	if ((*claim = (clonefd >= 0)))
		munmap(cloneview, filesize);
	return 0;
}

static ld_plugin_message message;
static ld_plugin_get_view get_view;
static ld_plugin_add_symbols add_symbols;
static ld_plugin_add_input_file add_input_file;

static enum ld_plugin_status
error(const char *fmt, ...)
{
	char buf[256];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, sizeof buf, fmt, va);
	va_end(va);
	message(LDPL_FATAL, "%s", buf);
	return LDPL_FATAL;
}

static enum ld_plugin_status
compat_get_view(const struct ld_plugin_input_file *file, const void **viewp)
{
	if (get_view)
		return get_view(file->handle, viewp);
	void *view = malloc(file->filesize);
	if (!view) return LDPS_ERR;
	*viewp = view;
	off_t sz = file->filesize;
	return pread(file->fd, view, sz, file->offset) != sz ? LDPS_ERR : 0;
}

static enum ld_plugin_status
claim_file_handler(const struct ld_plugin_input_file *file, int *claimed)
{
	int nplugsyms = 0;
	struct ld_plugin_symbol *plugsyms = 0;
	const char *filename = file->name;
	const void *view;
	enum ld_plugin_status status;
	if ((status = compat_get_view(file, &view)))
		return error("%s: get_view: %d", filename, status);

	const char *errmsg
	    = process_elf(file->fd, file->offset, file->filesize, view,
			  &nplugsyms, &plugsyms, claimed);
	if (errmsg)
		return error("%s: %s", filename, errmsg);
	if (*claimed)
		if ((status = add_symbols(file->handle, nplugsyms, plugsyms)))
			return error("%s: add_symbols: %d", filename, status);
	//free(plugsyms);
	if (!get_view) free((void *)view);
	return 0;
}

static enum ld_plugin_status
all_symbols_read_handler(void)
{
	struct obj *prev = 0;
	for (struct obj *next, *o = objs; o; o = next)
		next = o->next, o->next = prev, prev = o;
	objs = prev;
	enum ld_plugin_status status;
	for (struct obj *o = objs; o; o = o->next)
		if ((status = add_input_file(o->filename)))
			return error("%s: add_input_file: %d", o->filename, status);
	return 0;
}

static enum ld_plugin_status
cleanup_handler (void)
{
	for (struct obj *o = objs; o; o = o->next)
		unlink(o->filename);
	return 0;
}

enum ld_plugin_status
onload(struct ld_plugin_tv *tv)
{
	__typeof(tv->tv_u) u[28] = { 0 };
	for (; tv->tv_tag; tv++)
		if (tv->tv_tag < sizeof u / sizeof *u)
			u[tv->tv_tag] = tv->tv_u;
	message = u[LDPT_MESSAGE].tv_message;
	if (!message)
		return LDPL_FATAL;
	if (u[LDPT_API_VERSION].tv_val != LD_PLUGIN_API_VERSION)
		return error("linker plugin API version mismatch");

	get_view = u[LDPT_GET_VIEW].tv_get_view;
	add_symbols = u[LDPT_ADD_SYMBOLS].tv_add_symbols;
	add_input_file = u[LDPT_ADD_INPUT_FILE].tv_add_input_file;
	u[LDPT_REGISTER_CLAIM_FILE_HOOK].tv_register_claim_file
	    (claim_file_handler);
	u[LDPT_REGISTER_ALL_SYMBOLS_READ_HOOK].tv_register_all_symbols_read
	    (all_symbols_read_handler);
	u[LDPT_REGISTER_CLEANUP_HOOK].tv_register_cleanup(cleanup_handler);
	const char *symfile = u[LDPT_OPTION].tv_string;
	if (!symfile)
		return error("no input file");
	const char *errmsg = read_syms(symfile);
	if (errmsg)
		return error("%s: %s", symfile, errmsg);
	return 0;
}
_Static_assert(sizeof((ld_plugin_onload){onload}) != 0, "");
