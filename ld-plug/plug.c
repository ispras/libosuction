#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>

#include <elf.h>

#include "md5.h"
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

static int sockfd;
static struct {
	const char *output_name;
	const char *entrypoint;
	int is_dso;
	int nobj;
	int next_scn_uid;
	struct objfile {
		struct objfile *next;
		const char *name;
		const char *srcid;
		off_t offset;
		struct section {
			struct objfile *object;
			const char *name;
		        size_t size;
			struct sym {
				const char *name;
				struct section *section;
				struct rel *firstrel;
				enum {W_STRONG, W_COMMON, W_WEAK, W_UNDEF} weak;
				enum {V_DEFAULT, V_PROTECTED, V_HIDDEN} vis;
			} anchorsym;
			struct rel {
				struct section *section;
				struct sym *sym;
				struct rel *nextrel;
			} *rels;
			size_t nrels;
			struct sym **symptrs;
			size_t nsyms;
			int used;
			int shndx;
			int next;
			int uid;
		} *sections;
		int num_sections;
		int uids;
		struct sym *syms;
		int nsyms;
	} *obj_list;
} dg_info;

static struct symsht {
	unsigned *hashes;
	struct sym **syms;
	size_t size;
	size_t used;
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
static void
sym_htab_insert(unsigned hash, struct sym *sym)
{
	for (size_t i = hash; ; i++) {
		i &= syms_htab.size - 1;
		if (!syms_htab.syms[i]) {
			syms_htab.hashes[i] = hash;
			syms_htab.syms[i] = sym;
			return;
		}
	}
}
static void
sym_htab_expand(void)
{
	struct symsht oldht = syms_htab;
	syms_htab.used = oldht.used;
	syms_htab.size = oldht.size ? 2 * oldht.size : 2;
	syms_htab.hashes = calloc(syms_htab.size, sizeof *syms_htab.hashes);
	syms_htab.syms = calloc(syms_htab.size, sizeof *syms_htab.syms);
	for (size_t i = 0; i < oldht.size; i++)
		if (oldht.syms[i])
			sym_htab_insert(oldht.hashes[i], oldht.syms[i]);
	free(oldht.hashes);
	free(oldht.syms);
}
static struct sym **
sym_htab_lookup(const char *name)
{
	if (syms_htab.used * 3 >= syms_htab.size)
		sym_htab_expand();
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
	if (!syms_htab.used)
		return 0;
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

static void
dg_begin(const char *filename, int is_dso, int sockfd_, const char *entry)
{
	dg_info.output_name = strdup(filename);
	dg_info.entrypoint = *entry ? strdup(entry) : "_start";
	dg_info.is_dso = is_dso;
	sockfd = sockfd_;
}
static void
dg_print_obj(FILE *f, const struct objfile *o, int subgraph)
{
	static int clusterid;
	if (subgraph)
		fprintf(f, "subgraph cluster_o_%d { ", clusterid++);
	else
		fprintf(f, "digraph \"\" { ");
	fprintf(f, "label=\"%s\";\n", o->name);
	struct section *s;
	for (s = o->sections; s < o->sections + o->num_sections; s++) {
		if (!s->name) continue;
		if (!s->used) continue;
		fprintf(f, "\tsubgraph cluster_s_%d { ", s->uid);
		fprintf(f, "label=\"%s: size %zd\";\n", s->name, s->size);
		fprintf(f, "\t\tsection_%d[shape=plain,label=\".\"];\n", s->uid);
		fprintf(f, "\t}\n");
	}
	fprintf(f, "}\n");
}
static void
dg_print_rels(FILE *f, const struct objfile *o)
{
	struct section *s;
	for (s = o->sections; s < o->sections + o->num_sections; s++) {
		if (!s->used) continue;
		int sid = s->uid;
		for (struct rel *r = s->rels; r < s->rels + s->nrels; r++) {
			if (!r->sym || !r->sym->section) continue;
			int rid = r->sym->section->uid;
			fprintf(f, "\tsection_%d->section_%d[", sid, rid);
			if (sid != rid)
				fprintf(f, "ltail=cluster_s_%d,lhead=cluster_s_%d,",
					sid, rid);
			if (r->sym->name)
				fprintf(f, "tooltip=\"%s\"", r->sym->name);
			fprintf(f, "];\n");
		}
	}
}
static void
dg_print_all_graphviz(FILE *f)
{
	fprintf(f, "digraph \"\" { ");
	fprintf(f, "label=\"%s\";\n", dg_info.output_name);
	for (struct objfile *o = dg_info.obj_list; o; o = o->next)
		dg_print_obj(f, o, 1);
	for (struct objfile *o = dg_info.obj_list; o; o = o->next)
		dg_print_rels(f, o);
	fprintf(f, "}\n");
}
static void
dg_print(FILE *f)
{
	int nobj = dg_info.nobj, nscn = dg_info.next_scn_uid;
	fprintf(f, "%c %d %d %s %s\n", "REDP"[dg_info.is_dso], nobj, nscn,
		dg_info.output_name, dg_info.entrypoint);
	int *scndeps = calloc(nscn, sizeof *scndeps);
	for (struct objfile *o = dg_info.obj_list; o; o = o->next) {
		fprintf(f, "%d\t%lld\t%s\t%s\n",
			o->uids, (long long)o->offset, o->name, o->srcid);
		for (struct section *s = o->sections;
		     s < o->sections + o->num_sections; s++)
			if (s->name) {
				fprintf(f, "\t%d\t%zd\t%s\t%d\n", s->used, s->size, s->name, s->uid);
				int ndeps = 0;
				for (struct rel *r = s->rels;
				     r < s->rels + s->nrels; r++)
					if (r->sym && r->sym->section)
						ndeps += !scndeps[r->sym->section->uid]++;
				fprintf(f, "\t\t%d ", ndeps);
				for (struct rel *r = s->rels;
				     r < s->rels + s->nrels; r++)
					if (r->sym && r->sym->section)
						if (!--scndeps[r->sym->section->uid])
							fprintf(f, "%d ", r->sym->section->uid);
				fprintf(f, "\n");
			}
	}
	free(scndeps);
	int nsym = 0;
	for (struct objfile *o = dg_info.obj_list; o; o = o->next)
		for (struct sym *s = o->syms; s < o->syms + o->nsyms; s++)
			nsym += !!s->name;
	fprintf(f, "%d\n", nsym);
	for (struct objfile *o = dg_info.obj_list; o; o = o->next)
		for (struct sym *s = o->syms; s < o->syms + o->nsyms; s++) {
			if (!s->name)
				continue;
			fprintf(f, "%c%c %d\t%s\n",
			       "DCWU"[s->weak], "dph"[s->vis],
			       s->section ? s->section->uid : -1, s->name);
			struct objfile *thiso = s->section ? s->section->object : 0;
			int ndeps = 0;
			for (struct rel *r = s->firstrel; r; r = r->nextrel)
				if (r->section->object != thiso)
					ndeps++;
			fprintf(f, "\t%d ", ndeps);
			for (struct rel *r = s->firstrel; r; r = r->nextrel)
				if (r->section->object != thiso)
					fprintf(f, "%d ", r->section->uid);
			fprintf(f, "\n");
		}
}
static void
dg_end(void)
{
	struct objfile *prev = 0;
	for (struct objfile *o = dg_info.obj_list, *n; o; o = n)
		n = o->next, o->next = prev, prev = o;
	dg_info.obj_list = prev;
	//dg_print_all_graphviz(stdout);
	FILE *f = fdopen(sockfd, "w");
	dg_print(f);
	fclose(f);
}
static void
dg_object_start(const char *filename, off_t offset, int num_sections)
{
	struct objfile *o = malloc(sizeof *o);
	o->next = dg_info.obj_list;
	o->name = strdup(filename);
	o->srcid = 0;
	o->offset = offset;
	o->sections = calloc(num_sections, sizeof *o->sections);
	o->num_sections = num_sections;
	o->uids = 0;
	dg_info.obj_list = o;
	dg_info.nobj++;
}
static struct section *
dg_section(int n)
{
	return dg_info.obj_list->sections + n;
}
static void
dg_section_init(struct section *s, const char *name, size_t size, int used)
{
	s->object = dg_info.obj_list;
	s->name = name;
	s->size = size;
	s->anchorsym.section = s;
	static const char keep[] = "tors\0tors\0     ini\0           nit";
	unsigned o = name[1] - (unsigned)'c';
	if (!used && size && *name=='.' && o<7 && !strcmp(name+2, keep + o*5))
		used = 1;
	s->used = used;
	dg_info.obj_list->uids++;
	s->uid = dg_info.next_scn_uid++;
}
static struct sym *
dg_syms_alloc(size_t nsyms)
{
	struct objfile *o = dg_info.obj_list;
	o->nsyms = nsyms;
	return o->syms = calloc(nsyms, sizeof *o->syms);
}
static struct rel *
dg_rels_alloc(struct section *s, size_t nrels)
{
	s->nrels = nrels;
	return s->rels = calloc(nrels, sizeof *s->rels);
}
static void
dg_rel_init(struct rel *r, struct section *scn, struct sym *sym)
{
	if (sym->firstrel && sym->firstrel->section == scn)
		return;
	r->section = scn;
	r->sym = sym;
	r->nextrel = sym->firstrel;
	sym->firstrel = r;
}
static void
dg_object_end()
{
}

static int
dg_record_srcid(const char *name)
{
	if (strncmp (name, PLUG_SECTION_PREFIX, strlen (PLUG_SECTION_PREFIX)))
		return 0;
	dg_info.obj_list->srcid = name + strlen (PLUG_SECTION_PREFIX);
	return 1;
}
static void
dg_gen_srcid(const unsigned char *view, size_t len)
{
	if (dg_info.obj_list->srcid) return;
	unsigned char md5[16];
	md5_buffer(view, len, md5);
	char *srcid = malloc(33);
	srcid[32] = 0;
	printmd5(srcid, md5);
	dg_info.obj_list->srcid = srcid;
}
static void *
memdup(const void *p, size_t l)
{
	void *r = malloc(l);
	return r ? memcpy(r, p, l) : r;
}
static int
sym_vis(int elfvis)
{
	if (!elfvis) return V_DEFAULT;
	return elfvis == STV_PROTECTED ? V_PROTECTED : V_HIDDEN;
}
static int
sym_weak(int elfbind, int elfscn)
{
	if (!elfscn) return W_UNDEF;
	if (elfscn == SHN_COMMON) return W_COMMON;
	if (elfbind == STB_GNU_UNIQUE) elfbind = STB_WEAK;
	return elfbind == STB_WEAK ? W_WEAK : W_STRONG;
}
static const char *
process_elf(const char *filename, off_t offset, off_t filesize,
	    const unsigned char *view)
{
	const ElfNN_(Ehdr) *ehdr = (void *)view;
	typedef ElfNN_(Shdr) Shdr;
	typedef ElfNN_(Sym) Sym;
	typedef ElfNN_(Rel) Rel;
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
	dg_object_start(filename, offset, shnum);
	int shstrndx = ehdr->e_shstrndx;
	if (shstrndx == SHN_XINDEX)
		shstrndx = shdrs[0].sh_link;
	const char *shstrtab = (void *)(view + shdrs[shstrndx].sh_offset);
	shstrtab = memdup(shstrtab, shdrs[shstrndx].sh_size);
	size_t symsz = 0;
	int symtabidx = 0, relidx = 0;
	for (int i = 1; i < shnum; i++) {
		Shdr *shdr = shdrs + i;
		int used = 0;
		switch (shdr->sh_type) {
		case SHT_INIT_ARRAY: case SHT_FINI_ARRAY:
		case SHT_PREINIT_ARRAY:
			used = 1;
		case SHT_NOTE:
			if (shdr->sh_flags == SHF_EXCLUDE
			    && dg_record_srcid (shstrtab + shdr->sh_name))
				continue;
		case SHT_PROGBITS: case SHT_NOBITS:
		case SHT_ARM_EXIDX:
			dg_section_init(dg_section(i),
					shstrtab + shdr->sh_name,
			                shdr->sh_size, used);
			continue;
		case SHT_SYMTAB_SHNDX:
			dg_section(shdr->sh_link)->shndx = i;
			continue;
		case SHT_SYMTAB:
			symsz += shdr->sh_size;
			if (shdr->sh_entsize != sizeof(Sym))
				return "Sym size mismatch";
			dg_section(i)->next = symtabidx;
			symtabidx = i;
			continue;
		case SHT_REL: case SHT_RELA:
			dg_section(i)->next = relidx;
			relidx = i;
		case SHT_NULL: case SHT_STRTAB: case SHT_GROUP:
		case SHT_ARM_ATTRIBUTES:
			continue;
		case SHT_HASH: case SHT_DYNAMIC: case SHT_SHLIB:
		case SHT_DYNSYM:
			return "unexpected section type";
		default: return "unhandled section type";
		}
	}
	dg_gen_srcid(view, filesize);
	struct sym *syms = dg_syms_alloc(symsz / sizeof(Sym));
	for (int i = symtabidx; i; i = dg_section(i)->next) {
		Shdr *shdr = shdrs + i;
		Shdr *shndx = shdrs + dg_section(i)->shndx;
		Shdr *strtab = shdrs + shdr->sh_link;
		Sym *esyms = (void *)(view + shdr->sh_offset);
		const char *strings = (void *)(view + strtab->sh_offset);
		strings = memdup(strings, strtab->sh_size);
		unsigned *shndxs = (shndx == shdrs ? 0
				    : (void *)(view + shndx->sh_offset));
		size_t nsyms = shdr->sh_size / sizeof(Sym);
		struct sym **symptrs = malloc(nsyms * sizeof *symptrs);
		dg_section(i)->nsyms = nsyms;
		dg_section(i)->symptrs = symptrs;
		for (size_t j = 0; j < nsyms; j++) {
			Sym *sym = esyms + j;
			int bind = ELF_ST_BIND(sym->st_info);
			struct sym ssym = {0};
			ssym.name = strings + sym->st_name;
			int shndx = sym->st_shndx;
			if (shndx == SHN_XINDEX)
				if (shndxs)
					shndx = shndxs[j];
				else
					return "missing symtab_shndx";
			ssym.weak = sym_weak(bind, shndx);
			if (shndx == SHN_COMMON || shndx == SHN_ABS) shndx = 0;
			ssym.section = shndx ? dg_section(shndx) : 0;
			ssym.vis = sym_vis(ELF_ST_VISIBILITY(sym->st_other));

			if (bind == STB_LOCAL) {
				symptrs[j] = ssym.section ? &ssym.section->anchorsym : 0;
				continue;
			}
			if (!sym->st_name)
				return "unnamed non-local symbol";

			struct sym **symslot = sym_htab_lookup(ssym.name);
			if (!*symslot) {
				*syms = ssym;
				*symslot = syms++;
				syms_htab.used++;
			} else if (!ssym.weak && !(*symslot)->weak) {
				return "duplicate definition";
			} else if (ssym.weak < (*symslot)->weak) {
				ssym.firstrel = (*symslot)->firstrel;
				**symslot = ssym;
			}
			symptrs[j] = *symslot;
		}
	}
	for (int i = relidx; i; i = dg_section(i)->next) {
		Shdr *shdr = shdrs + i;
		struct section *relscn = dg_section(shdr->sh_info);
		if (!relscn->name)
			return "relocations applied to unexpected section";
		struct section *symtabscn = dg_section(shdr->sh_link);
		if (!symtabscn->symptrs)
			return "symtab not populated";
		ssize_t nrels = shdr->sh_size / shdr->sh_entsize;
		struct rel *r = dg_rels_alloc(relscn, nrels);
		for (ssize_t j=0, o=0; j < nrels; j++, r++, o+=shdr->sh_entsize) {
			Rel *rel = (void *)(view + shdr->sh_offset + o);
			struct sym *sym = symtabscn->symptrs[ELF_R_SYM(rel->r_info)];
			dg_rel_init(r, relscn, sym);
		}
	}
	for (int i = symtabidx; i; i = dg_section(i)->next)
		free(dg_section(i)->symptrs);
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
	*claimed = 0;
	const char *filename = file->name;
	const void *view;
	enum ld_plugin_status status;
	if ((status = compat_get_view(file, &view)))
		return error("%s: get_view: %d", filename, status);

	const char *errmsg = process_elf(filename, file->offset, file->filesize,
					 view);
	if (errmsg)
		return error("%s: %s", filename, errmsg);
	if (!get_view) free((void *)view);
	return 0;
}

static enum ld_plugin_status
all_symbols_read_handler(void)
{
	dg_end();
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
	u[LDPT_REGISTER_CLAIM_FILE_HOOK].tv_register_claim_file
	    (claim_file_handler);
	u[LDPT_REGISTER_ALL_SYMBOLS_READ_HOOK].tv_register_all_symbols_read
	    (all_symbols_read_handler);

	const char *optstr = u[LDPT_OPTION].tv_string, *entry;
	if (!optstr || !(entry = strchr(optstr, ':')))
		return error("plugin options missing");
	int sockfd = atoi(optstr);

	int output = u[LDPT_LINKER_OUTPUT].tv_val;
	dg_begin(u[LDPT_OUTPUT_NAME].tv_string, output, sockfd, entry+1);
	return 0;
}
_Static_assert(sizeof((ld_plugin_onload){onload}) != 0, "");
