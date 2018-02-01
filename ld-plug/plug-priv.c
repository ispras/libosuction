#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <elf.h>

#include "elf-common.h"
#include "md5.h"

#define HAVE_STDINT_H
#include <plugin-api.h>
_Static_assert(LD_PLUGIN_API_VERSION == 1, "unexpected plugin API version");

static const char *linkid_err;

struct sym {
	const char *name;
	unsigned srcid[4];
};

static struct symsht {
	unsigned *hashes;
	struct sym **syms;
	size_t size;
} syms_htab;

static void
sym_htab_alloc(size_t sz)
{
	while (sz & (sz-1)) sz &= sz-1;
	sz *= 4;
	syms_htab.hashes = calloc(sz, sizeof *syms_htab.hashes);
	syms_htab.syms = calloc(sz, sizeof *syms_htab.syms);
	syms_htab.size = sz;
}
static inline unsigned
sym_hash(const char *name, unsigned *id)
{
	const unsigned char *c = (const void *)name;
	unsigned h = id[0] ^ id[1] ^ id[2] ^ id[3];
	for (; *c; c++)
		h += h*32 + *c;
	return h;
}
static struct sym **
sym_htab_lookup(const char *name, unsigned *id)
{
	unsigned hash = sym_hash(name, id);
	for (size_t i = hash; ; i++) {
		i &= syms_htab.size - 1;
		if (!syms_htab.syms[i]) {
			syms_htab.hashes[i] = hash;
			return syms_htab.syms + i;
		}
		if (syms_htab.hashes[i] == hash
		    && !memcmp(id, syms_htab.syms[i]->srcid, 4 * sizeof *id)
		    && !strcmp(name, syms_htab.syms[i]->name))
			return syms_htab.syms + i;
	}
}
static const char *
read_syms(const char *linkid, const char *file)
{
	FILE *f = fopen(file, "r");
	if (!f)
		return "error opening file";
	int nelim, nloc, nhid, nund;
	char id[33];
	int found = 0;
	while (fscanf(f, "%d %d %d %d %32s", &nelim, &nloc, &nhid, &nund, id)
	       == 5) {
		if (strcmp(id, linkid)) {
			for (int i = 0; i < nelim + nloc + nhid + nund; i++)
				fscanf(f, " %*[^\n]");
			continue;
		}
		sym_htab_alloc(nelim + nloc + nhid);
		for (int i = 0; i < nelim + nloc + nhid; i++) {
			struct sym s;
			fscanf(f, " %*[^:]:%32[0-9a-f]:%*d:%*c:%ms", id, &s.name);
			unsigned *t = s.srcid;
			sscanf(id, "%8x%8x%8x%8x", t+0, t+1, t+2, t+3);
			struct sym **symp = sym_htab_lookup(s.name, s.srcid);
			if (*symp) return "duplicate symbol";
			*symp = malloc(sizeof **symp);
			**symp = s;
		}
		found = 1;
		break;
	}
	fclose(f);
	if (!found)
		linkid_err = linkid;
	return 0;
}
static char *
plug_gen_srcid(char srcid[], const unsigned char *view, size_t len)
{
	unsigned char md5[16];
	md5_buffer(view, len, md5);
	srcid[32] = 0;
	printmd5(srcid, md5);
	return srcid;
}
static const char *
process_elf(const char *filename, int fd, off_t offset, off_t filesize,
	    const unsigned char *view)
{
	const ElfNN_(Ehdr) *ehdr = (void *)view;
	typedef ElfNN_(Shdr) Shdr;
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
	const char *shstrtab = (void *)(view + shdrs[shstrndx].sh_offset);
	const char *srcid = 0;
	for (int i = 1; i < shnum; i++) {
		Shdr *shdr = shdrs + i;
		if (shdr->sh_type == SHT_NOTE
		    && shdr->sh_flags == SHF_EXCLUDE
		    && (srcid = plug_srcid(shstrtab + shdr->sh_name)))
			break;
	}
	if (!srcid) srcid = plug_gen_srcid((char[33]){}, view, filesize);
	/* fprintf is lower level of abstraction than error/message, but we use
	 * it here since we don't want every srcid to be prefixed with
	 * e.g. 'ld.bfd: ...', plus gold would exit on message(LDPL_FATAL,..). */
	if (linkid_err)
		fprintf(stderr, "srcid[%s@%lld] = %s\n", filename,
			(long long)offset, srcid);
	return 0;
}

static ld_plugin_message message;
static ld_plugin_get_view get_view;

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
	const char *filename = file->name;
	const void *view;
	enum ld_plugin_status status;
	if ((status = compat_get_view(file, &view)))
		return error("%s: get_view: %d", filename, status);

	const char *errmsg
		= process_elf(filename, file->fd, file->offset, file->filesize,
			      view);
	if (errmsg)
		return error("%s: %s", filename, errmsg);
	if (!get_view) free((void *)view);
	*claimed = 0;
	return 0;
}

static enum ld_plugin_status
all_symbols_read_handler(void)
{
	if (linkid_err)
		return error("could not find linkid %s", linkid_err);
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
	const char *optstr = u[LDPT_OPTION].tv_string, *linkid, *symfile;
	if (!optstr)
		return error("no input file");
	if (sscanf(optstr, "%m[0-9a-f]:%ms", &linkid, &symfile) != 2)
		return error("bad plugin option format");
	const char *errmsg = read_syms(linkid, symfile);
	if (errmsg)
		return error("%s: %s", symfile, errmsg);
	if (!syms_htab.size && !linkid_err) return 0;
	u[LDPT_REGISTER_CLAIM_FILE_HOOK].tv_register_claim_file
	    (claim_file_handler);
	u[LDPT_REGISTER_ALL_SYMBOLS_READ_HOOK].tv_register_all_symbols_read
	    (all_symbols_read_handler);
	return 0;
}
_Static_assert(sizeof((ld_plugin_onload){onload}) != 0, "");
