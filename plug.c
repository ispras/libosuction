#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <elf.h>

#define HAVE_STDINT_H
#include <plugin-api.h>
_Static_assert(LD_PLUGIN_API_VERSION == 1, "unexpected plugin API version");

#define ElfNN_(t) Elf64_##t
#define ELFCLASS ELFCLASS64
#define ELFDATA ELFDATA2LSB
#define ELF_ST_BIND(st_info)        ELF64_ST_BIND(st_info)
#define ELF_ST_VISIBILITY(st_other) ELF64_ST_VISIBILITY(st_other)
#define ELF_R_SYM(r_info)           ELF64_R_SYM(r_info)

static struct {
	const char *output_name;
	struct objfile {
		struct objfile *next;
		const char *name;
		struct section {
			struct objfile *object;
			const char *name;
		        size_t size;
			struct rel {
				const char *name;
				int symscn;
			} *rels;
			size_t nrels;
			struct sym **symptrs;
			size_t nsyms;
			int used;
			int shndx;
			int next;
			int graphid;
		} *sections;
		int num_sections;
		struct sym {
			const char *name;
			struct section *section;
			enum {W_STRONG, W_COMMON, W_WEAK, W_UNDEF} weak;
			enum {V_DEFAULT, V_PROTECTED, V_HIDDEN} vis;
		} *syms;
		int nsyms;
	} *obj_list;
} dg_info;

static struct symsht {
	unsigned *hashes;
	struct sym **syms;
	size_t size;
	size_t used;
} syms_htab;

static unsigned
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
	syms_htab.size = oldht.size ? 2 * oldht.size : 1;
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

static void
dg_begin(const char *filename)
{
	dg_info.output_name = filename;
}
static int
rels_cmp(const void *va, const void *vb)
{
	const struct rel *a = va, *b = vb;
	if (a->symscn - b->symscn != 0)
		return a->symscn - b->symscn;
	return strcmp(a->name ? a->name : "", b->name ? b->name : "");
}
static void
dg_print_obj(FILE *f, const struct objfile *o, int subgraph)
{
	static int clusterid, sectionid;
	if (subgraph)
		fprintf(f, "subgraph cluster_o_%d { ", clusterid++);
	else
		fprintf(f, "digraph { ");
	fprintf(f, "label=\"%s\";\n", o->name);
	struct section *s;
	for (s = o->sections; s < o->sections + o->num_sections; s++) {
		if (!s->used) continue;
		int id = s->graphid = sectionid++;
		fprintf(f, "\tsubgraph cluster_s_%d { ", id);
		fprintf(f, "label=\"%s: size %zd\";\n", s->name, s->size);
		fprintf(f, "\t\tsection_%d[shape=plain,label=\".\"];\n", id);
		fprintf(f, "\t}\n");
	}
	for (s = o->sections; s < o->sections + o->num_sections; s++) {
		if (!s->used) continue;
		qsort(s->rels, s->nrels, sizeof *s->rels, rels_cmp);
		int sid = s->graphid, rid, prevrid = 0;
		for (struct rel *r = s->rels; r < s->rels + s->nrels; r++) {
			rid = o->sections[r->symscn].graphid;
			if (!r->symscn || rid == prevrid) continue;
			prevrid = rid;
			fprintf(f, "\tsection_%d->section_%d[", sid, rid);
			if (r->symscn != s-o->sections)
				fprintf(f, "ltail=cluster_s_%d,lhead=cluster_s_%d,",
					sid, rid);
			if (r->name)
				fprintf(f, "tooltip=\"%s\"", r->name);
			fprintf(f, "];\n");
		}
	}
	fprintf(f, "}\n");
}
static void
dg_print_all(FILE *f)
{
	fprintf(f, "digraph { ");
	fprintf(f, "label=\"%s\";\n", dg_info.output_name);
	for (struct objfile *o = dg_info.obj_list; o; o = o->next)
		dg_print_obj(f, o, 1);
	fprintf(f, "}\n");
}
static void
dg_end(void)
{
	dg_print_all(stdout);
}
static void
dg_object_start(const char *filename, int num_sections)
{
	struct objfile *o = malloc(sizeof *o);
	o->next = dg_info.obj_list;
	o->name = filename;
	o->sections = calloc(num_sections, sizeof *o->sections);
	o->num_sections = num_sections;
	dg_info.obj_list = o;
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
	static const char keep[] = "tors\0tors\0     ini\0           nit";
	unsigned o = name[1] - (unsigned)'c';
	if (!used && size && *name=='.' && o<7 && !strcmp(name+2, keep + o*5))
		used = 1;
	s->used = used;
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
static struct rel *
dg_rel(int n, struct section *s)
{
	return s->rels + n;
}
static void
dg_rel_init(struct rel *r, struct section *s, int symscn, const char *name)
{
	if (!symscn || symscn==SHN_COMMON) return;
	r->name = name;
	r->symscn = symscn;
	s->used = dg_section(symscn)->used = 1;
}
static void
dg_object_end()
{
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
	return elfbind == STB_WEAK ? W_WEAK : W_STRONG;
}
static const char *
process_elf(const char *filename, const unsigned char *view)
{
	const ElfNN_(Ehdr) *ehdr = (void *)view;
	typedef ElfNN_(Shdr) Shdr;
	typedef ElfNN_(Sym) Sym;
	typedef ElfNN_(Rel) Rel;
	typedef ElfNN_(Rela) Rela;
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
	dg_object_start(filename, shnum);
	int shstrndx = ehdr->e_shstrndx;
	if (shstrndx == SHN_XINDEX)
		shstrndx = shdrs[0].sh_link;
	const char *shstrtab = (void *)(view + shdrs[shstrndx].sh_offset);
	size_t symsz = 0;
	int symtabidx = 0, relidx = 0;
	for (int i = 1; i < shnum; i++) {
		Shdr *shdr = shdrs + i;
		int used = 0;
		switch (shdr->sh_type) {
		case SHT_INIT_ARRAY: case SHT_FINI_ARRAY:
		case SHT_PREINIT_ARRAY:
			used = 1;
		case SHT_PROGBITS: case SHT_NOBITS:
			dg_section_init(dg_section(i),
					shstrtab + shdr->sh_name,
			                shdr->sh_size, used);
			continue;;
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
		case SHT_NULL: case SHT_STRTAB: case SHT_NOTE:
			continue;
		case SHT_HASH: case SHT_DYNAMIC: case SHT_SHLIB:
		case SHT_DYNSYM:
			return "unexpected section type";
		case SHT_GROUP:
		default: return "unhandled section type";
		}
	}
	struct sym *syms = dg_syms_alloc(symsz / sizeof(Sym));
	for (int i = symtabidx; i; i = dg_section(i)->next) {
		Shdr *shdr = shdrs + i;
		Shdr *shndx = shdrs + dg_section(i)->shndx;
		Shdr *strtab = shdrs + shdr->sh_link;
		Sym *esyms = (void *)(view + shdr->sh_offset);
		const char *strings = (void *)(view + strtab->sh_offset);
		unsigned *shndxs = (shndx == shdrs ? 0
				    : (void *)(view + shndx->sh_offset));
		size_t nsyms = shdr->sh_size / sizeof(Sym);
		struct sym **symptrs = malloc(nsyms * sizeof *symptrs);
		dg_section(i)->nsyms = nsyms;
		dg_section(i)->symptrs = symptrs;
		for (size_t j = 0; j < nsyms; j++) {
			Sym *sym = esyms + j;
			int bind = ELF_ST_BIND(sym->st_info);
			if (bind == STB_LOCAL)
				continue;
			if (!sym->st_name)
				return "unnamed non-local symbol";
			struct sym ssym;
			ssym.name = strings + sym->st_name;
			int shndx = sym->st_shndx;
			if (shndx == SHN_XINDEX)
				if (shndxs)
					shndx = shndxs[j];
				else
					return "missing symtab_shndx";
			ssym.weak = sym_weak(bind, shndx);
			if (shndx == SHN_COMMON) shndx = 0;
			ssym.section = shndx ? dg_section(shndx) : 0;
			ssym.vis = sym_vis(ELF_ST_VISIBILITY(sym->st_other));

			struct sym **symslot = sym_htab_lookup(ssym.name);
			if (!*symslot) {
				*syms = ssym;
				*symslot = syms++;
				syms_htab.used++;
			} else if (!ssym.weak && !(*symslot)->weak)
				return "duplicate definition";
			else if (ssym.weak < (*symslot)->weak)
				**symslot = ssym;
			symptrs[j] = *symslot;
		}
	}
	for (int i = relidx; i; i = dg_section(i)->next) {
		Shdr *shdr = shdrs + i;
		Shdr *symtab = shdrs + shdr->sh_link;
		Shdr *symtab_shndx = shdrs + dg_section(shdr->sh_link)->shndx;
		Shdr *strtab = shdrs + symtab->sh_link;
		Sym *syms = (void *)(view + symtab->sh_offset);
		const char *strings = (void *)(view + strtab->sh_offset), *name;
		unsigned *shndxs = (symtab_shndx == shdrs ? 0
				    : (void *)(view + symtab_shndx->sh_offset));
		struct section *relscn = dg_section(shdr->sh_info);
		if (!relscn->name)
			return "initcheck";
		ssize_t nrels = shdr->sh_size / shdr->sh_entsize;
		struct rel *r = dg_rels_alloc(relscn, nrels);
		for (ssize_t j=0, o=0; j < nrels; j++, r++, o+=shdr->sh_entsize) {
			Rel *rel = (void *)(view + shdr->sh_offset + o);
			Sym *sym = syms + ELF_R_SYM(rel->r_info);
			name = sym->st_name ? strings + sym->st_name : 0;
			int shndx = sym->st_shndx;
			if (shndx == SHN_XINDEX)
				if (shndxs)
					shndx = shndxs[ELF_R_SYM(rel->r_info)];
				else
					return "missing symtab_shndx";
			int bind = ELF_ST_BIND(sym->st_info);
			int vis = ELF_ST_VISIBILITY(sym->st_other);
			dg_rel_init(r, relscn, shndx, name);
		}
	}
	for (int i = symtabidx; i; i = dg_section(i)->next)
		free(dg_section(i)->symptrs);
	for (int i = shnum - 1; i > 0; i--) {
		Shdr *shdr = shdrs + i;
		ElfNN_(Sym) *syms, *sym;
		const char *strtab;
		const char *refsec;
		switch (shdr->sh_type) {
		case SHT_SYMTAB:
			sym = syms = (void *)(view + shdr->sh_offset);
			strtab = (void *)(view + shdrs[shdr->sh_link].sh_offset);
			for (uint64_t o = 0; o < shdr->sh_size; sym++, o += sizeof *sym) {
				int bind = ELF_ST_BIND (sym->st_info);
				int vis = ELF_ST_VISIBILITY (sym->st_other);
				if (!sym->st_name
				    || !sym->st_shndx
				    || sym->st_shndx >= SHN_LORESERVE
				    || 0 && bind == STB_LOCAL
				    || 0 && bind == STB_WEAK
				    || 0 && vis == STV_INTERNAL
				    || 0 && vis == STV_HIDDEN)
					continue;
			}
			break;
		case SHT_RELA:
			syms = (void *)(view + shdrs[shdr->sh_link].sh_offset);
			strtab = (void *)(view + shdrs[shdrs[shdr->sh_link].sh_link].sh_offset);
			refsec = shstrtab + shdrs[shdr->sh_info].sh_name;
			if (shdr->sh_entsize != sizeof (ElfNN_(Rela)))
				return "Rela size mismatch";
			for (uint64_t o = 0; o < shdr->sh_size; o += shdr->sh_entsize) {
				ElfNN_(Rela) *rela = (void*)(view + shdr->sh_offset + o);
				sym = syms + ELF_R_SYM (rela->r_info);
				int bind = ELF_ST_BIND (sym->st_info);
				int vis = ELF_ST_VISIBILITY (sym->st_other);
				if (!sym->st_name
				    || 0 && bind == STB_LOCAL
				    || 0 && vis == STV_INTERNAL
				    || 0 && vis == STV_HIDDEN)
					continue;
			}
			break;
		case SHT_REL:
			break;
		}
	}
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
claim_file_handler(const struct ld_plugin_input_file *file, int *claimed)
{
	*claimed = 0;
	const char *filename = file->name;
	const void *view;
	enum ld_plugin_status status;
	if ((status = get_view(file->handle, &view)))
		return error("%s: get_view: %d", filename, status);

	const char *errmsg = process_elf(filename, view);
	if (errmsg)
		return error("%s: %s", filename, errmsg);
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
	dg_begin(u[LDPT_OUTPUT_NAME].tv_string);
	return 0;
}
_Static_assert(sizeof((ld_plugin_onload){onload}) != 0, "");
