#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "deps-graph.h"

static struct htab objs_htab, syms_htab;

unsigned
sym_hash(const char *name)
{
	const unsigned char *c = (const void *)name;
	unsigned h = 5381;
	for (; *c; c++)
		h += h*32 + *c;
	return h;
}
static inline unsigned
obj_hash(const char *srcid)
{
	unsigned h[4];
	sscanf(srcid, "%8x%8x%8x%8x", h+0, h+1, h+2, h+3);
	return h[0] ^ h[1] ^ h[2] ^ h[3];
}
static void
htab_insert(struct htab *htab, unsigned hash, void *elt)
{
	for (size_t i = hash; ; i++) {
		i &= htab->size - 1;
		if (!htab->elts[i]) {
			htab->hashes[i] = hash;
			htab->elts[i] = elt;
			return;
		}
	}
}
void
htab_expand(struct htab *htab)
{
	struct htab oldht = *htab;
	htab->used = oldht.used;
	htab->size = oldht.size ? 2 * oldht.size : 2;
	htab->hashes = calloc(htab->size, sizeof *htab->hashes);
	htab->elts = calloc(htab->size, sizeof *htab->elts);
	for (size_t i = 0; i < oldht.size; i++)
		if (oldht.elts[i])
			htab_insert(htab, oldht.hashes[i], oldht.elts[i]);
	free(oldht.hashes);
	free(oldht.elts);
}
static struct sym **
sym_htab_lookup(const char *name)
{
	if (syms_htab.used * 3 >= syms_htab.size)
		htab_expand(&syms_htab);
	unsigned hash = sym_hash(name);
	for (size_t i = hash; ; i++) {
		i &= syms_htab.size - 1;
		struct sym **ptr = (void *)(syms_htab.elts + i);
		if (!*ptr) {
			syms_htab.hashes[i] = hash;
			return ptr;
		}
		if (syms_htab.hashes[i] == hash
		    && !strcmp(name, (*ptr)->name))
			return ptr;
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
		struct sym *sym = syms_htab.elts[i];
		if (!sym)
			return 0;
		if (syms_htab.hashes[i] == hash
		    && !strcmp(name, sym->name))
			return sym;
	}
}
static struct obj **
obj_htab_lookup(const char *srcid)
{
	if (objs_htab.used * 3 >= objs_htab.size)
		htab_expand(&objs_htab);
	unsigned hash = obj_hash(srcid);
	for (size_t i = hash; ; i++) {
		i &= objs_htab.size - 1;
		struct obj **ptr = (void *)(objs_htab.elts + i);
		if (!*ptr) {
			objs_htab.hashes[i] = hash;
			return ptr;
		}
		if (objs_htab.hashes[i] == hash
		    && !strcmp(srcid, (*ptr)->srcid))
			return ptr;
	}
}
void
input_dyndeps(FILE *f)
{
	char *srcid, *scn, *sym, c;
	while (fscanf(f, "%*[^:]:%*d:%m[^:]:%m[^:]:%*[^:]:%*[^:]%c",
		      &srcid, &scn, &c) == 3 && c == ':') {
		struct obj **objp = obj_htab_lookup(srcid), *o = *objp;
		if (!*objp) {
			*objp = o = calloc(1, sizeof *o);
			o->srcid = srcid;
			o->srcidmain = o;
			objs_htab.used++;
		} else {
			free(srcid);
			srcid = (char *)o->srcid;
		}
		struct scn *s = 0;
		for (int i = 0; i < o->nscn; i++)
			if (!strcmp(scn, o->scns[i].name)) {
				s = o->scns + i;
				break;
			}
		if (!s) {
			o->scns = realloc(o->scns, ++o->nscn * sizeof *o->scns);
		        s = &o->scns[o->nscn - 1];
			memset(s, 0, sizeof *s);
			s->name = scn;
		} else {
			free(scn);
			scn = (char *)s->name;
		}
		do {
			if (fscanf(f, "%m[^,\n]%c", &sym, &c) != 2)
				break;
			struct sym **symp = sym_htab_lookup(sym), *y = *symp;
			if (!*symp) {
				*symp = y = calloc(1, sizeof *y);
				syms_htab.used++;
				y->name = sym;
				y->weak = 'U';
				y->vis = 'd';
				y->n.kind = N_SYM;
			} else {
				free(sym);
				sym = (char *)y->name;
			}
			s->n.out = realloc(s->n.out, ++s->n.nout * sizeof *s->n.out);
			s->n.out[s->n.nout - 1] = &y->n;
		} while (c == ',');
	}
}
struct scn *
find_dyndeps_scn(struct obj *o, const char *scn)
{
	if (o->path)
		return 0;
	for (int i = 0; i < o->nscn; i++)
		if (!strcmp(scn, o->scns[i].name))
			return o->scns + i;
	return 0;
}
__attribute__((unused))
static int
is_implicitly_used_section(const char *name)
{
	// FIXME: it's better to look at .gcc_except_table -> .text edges
	static const char lsda_scn[] = ".gcc_except_table";
	size_t lsda_scn_len = sizeof lsda_scn - 1;
	if (!strncmp(name, lsda_scn, lsda_scn_len))
		return !name[lsda_scn_len] || name[lsda_scn_len] == '.';
	return 0;
}
static int
section_may_have_extab(const char *name)
{
	// Noooo...
	if (!strncmp(name, ".text", 5))
		return !name[5] || name[5] == '.';
	return 0;
}
static int
section_is_extab_for(const char *xname, const char *tname)
{
	// Nooooooo...
	static const char *const xnames[] = {".gcc_except_table", ".ARM.extab", ".ARM.exidx"};
	for (int i = 0; i < sizeof xnames / sizeof *xnames; i++) {
		size_t len = strlen(xnames[i]);
		if (!strncmp(xname, xnames[i], len))
			return !strcmp(xname + len, tname + 5) || !strcmp(xname + len, tname);
	}
	return 0;
}
int
input(struct dso *dso, FILE *f)
{
	char is_dso;
	if (fscanf(f, " %c %d %d %ms %ms %ms", &is_dso, &dso->nobj, &dso->nscn,
		   &dso->name, &dso->entrypoint, &dso->linkid) != 6)
		return 1;
	dso->is_dso = is_dso;
	struct obj *o = dso->obj = calloc(dso->nobj, sizeof *dso->obj);
	struct scn *s = dso->scn = calloc(dso->nscn, sizeof *dso->scn);
	for (; o < dso->obj + dso->nobj; o++) {
		fscanf(f, "%d %lld %ms %ms", &o->nscn, &o->offset, &o->path, &o->srcid);
		o->srcidmain = o;
		if (o->srcid[0] != '-') {
			struct obj **objp = obj_htab_lookup(o->srcid);
			if (!*objp) {
				*objp = o;
				objs_htab.used++;
			}
			o->srcidmain = *objp;
			o->srcidmain->srcidcnt++;
		}
		o->scns = s;
		for (; s < o->scns + o->nscn; s++) {
			fscanf(f, "%d %lld %ms %*[^\n]", &s->used, &s->size, &s->name);
			s->objptr = o;
			struct scn *ds = find_dyndeps_scn(o->srcidmain, s->name);
			fscanf(f, "%d", &s->nscndeps);
			int alloc = s->nscndeps + !!ds;
			/* XXX extra slots for exceptions hack below: */
			if (section_may_have_extab(s->name))
				alloc += 2;
			if (!alloc) continue;
			s->scndeps = malloc(alloc * sizeof *s->scndeps);
			for (int i = 0; i < s->nscndeps; i++) {
				int t;
				fscanf(f, "%d", &t);
				s->scndeps[i] = dso->scn + t;
			}
			if (ds)
				s->scndeps[s->nscndeps++] = ds;
		}
		for (s = o->scns; s < o->scns + o->nscn; s++)
			if (section_may_have_extab(s->name)) {
				struct scn *x1 = s, *x2 = s;
				for (struct scn *ts = o->scns; ts < o->scns + o->nscn; ts++)
					if (section_is_extab_for(ts->name, s->name))
						x1 = x2, x2 = ts;
				s->scndeps[s->nscndeps++] = x1;
				s->scndeps[s->nscndeps++] = x2;
			}

	}
	fscanf(f, "%d", &dso->nsym);
	dso->entrysym = 0;
	struct sym *y = dso->sym = calloc(dso->nsym, sizeof *dso->sym);
	for (; y < dso->sym + dso->nsym; y++) {
		int t;
		fscanf(f, " %c%c%c %d %ms", &y->weak, &y->vis, &y->tls, &t, &y->name);
		char *at = strstr(y->name, "@@");
		if (at) *at = 0;
		y->n.kind = N_SYM;
		if (t >= 0) {
			y->defscn = &dso->scn[t].n;
			y->n.nout = 1;
			y->n.out = &y->defscn;
		}
		fscanf(f, "%d", &y->nrevdeps);
		y->revdeps = y->nrevdeps ? malloc(y->nrevdeps * sizeof *y->revdeps) : 0;
		for (int i = 0; i < y->nrevdeps; i++) {
			int t;
			fscanf(f, "%d", &t);
			y->revdeps[i] = dso->scn + t;
			dso->scn[t].nsymdeps++;
		}
		if (!y->n.out
		    && (!strncmp(y->name, "__start_", 8)
			|| !strncmp(y->name, "__stop_", 7))) {
			const char *scnname = y->name + 7 + (y->name[4] == 'a');
			int ndep = 0;
			for (s = dso->scn; s < dso->scn + dso->nscn; s++)
				ndep += !strcmp(s->name, scnname);
			if (ndep) {
				y->n.out = malloc(ndep * sizeof *y->n.out);
				y->n.nout = ndep; ndep = 0;
				for (s = dso->scn; s < dso->scn + dso->nscn; s++)
					if (!strcmp(s->name, scnname))
						y->n.out[ndep++] = &s->n;
				y->weak = 'C'; y->vis = 'h';
			}
		}
		if (!strcmp(y->name, dso->entrypoint))
			dso->entrysym = y;
		if (y->weak != 'U') continue;
		struct sym **symp = sym_htab_lookup(y->name);
		if (!*symp) {
			*symp = y;
			syms_htab.used++;
		}
		else {
			y->defscn = &(*symp)->n;
			y->n.nout = 1;
			y->n.out = &y->defscn;
		}
	}
	for (s = dso->scn; s < dso->scn + dso->nscn; s++)
		s->symdeps = malloc(s->nsymdeps * sizeof *s->symdeps);
	for (y = dso->sym; y < dso->sym + dso->nsym; y++)
		for (int i = 0; i < y->nrevdeps; i++) {
			s = y->revdeps[i];
			s->symdeps[0] = y;
			s->symdeps++;
		}
	for (s = dso->scn; s < dso->scn + dso->nscn; s++) {
		s->symdeps -= s->nsymdeps;
		s->n.nout = s->nscndeps + s->nsymdeps;
		struct node **p = s->n.out = malloc(s->n.nout * sizeof *s->n.out);
                for (int i = 0; i < s->nscndeps; i++)
			*p++ = (void *)s->scndeps[i];
		for (int i = 0; i < s->nsymdeps; i++)
			*p++ = (void *)s->symdeps[i];
	}
	return 0;
}

static void scc_1(struct node *n)
{
	static struct node *stackptr;
        static int preorderidx;
	n->lowlink = n->preorderidx = ++preorderidx;
	n->onstack = 1;
	n->stacknext = stackptr;
	stackptr = n;

	for (int i = 0; i < n->nout; i++) {
		struct node *o = n->out[i];
		if (!o->preorderidx) {
			scc_1(o);
			if (n->lowlink > o->lowlink)
				n->lowlink = o->lowlink;
		} else if (o->onstack) {
			if (n->lowlink > o->preorderidx)
				n->lowlink = o->preorderidx;
		}
	}
	if (n->lowlink != n->preorderidx) return;
	int sccsize = 0;
	for (struct node *o = stackptr; o != n; o = o->stacknext) {
		o->onstack = 0;
		o->sccroot = n;
		sccsize++;
	}
	n->onstack = 0;
	n->sccroot = n;
	n->sccsize = ++sccsize;
	if (1 && sccsize > 1) {
		printf("SCC size %d\n", sccsize);
		for (struct node *o = stackptr; sccsize; o = o->stacknext, sccsize--)
			switch (o->kind) {
			case N_SCN:
				printf("\tscn %s\n", ((struct scn *)o)->name);
				break;
			case N_SYM:
				printf("\tsym %s\n", ((struct sym *)o)->name);
				break;
			}
	}
	stackptr = n->stacknext;
}
static void scc_2(struct node *n, int *w, int g)
{
	if (n->kind == N_SCN) {
		struct scn *s = (void *)n;
		if (0||!strncmp(s->name, ".text", 5)) {
			//printf("%s:%d\n", s->name, s->size);
			*w += s->size;
		}
	}
	n->onstack = g;
	for (int i = 0; i < n->nout; i++) {
		struct node *o = n->out[i];
		if (o->onstack != g)
			scc_2(o, w, g);
	}
}

static void scc(struct dso *dso)
{
#if 1
	for (int i = 0; i < dso->nsym; i++) {
		int w = 0;
		scc_2(&dso->sym[i].n, &w, i+1);
		printf("%d %s\n", w, dso->sym[i].name);
	}
	return;
#endif
	for (int i = 0; i < dso->nscn; i++)
		if (!dso->scn[i].n.preorderidx)
			scc_1(&dso->scn[i].n);
	for (int i = 0; i < dso->nsym; i++)
		if (!dso->sym[i].n.preorderidx)
			scc_1(&dso->sym[i].n);
}

static void link(struct dso *dsos, int n)
{
	struct node *stack = 0;
	int sum = 0;
	for (struct dso *dso = dsos; dso < dsos + n; dso++)
		for (struct sym *y = dso->sym; y < dso->sym + dso->nsym; y++)
			if (y->weak != 'U' && y->vis != 'h') {
				struct sym *u;
				if (!(u=sym_htab_lookup_only(y->name))) continue;
				sum++;
				if (!u->n.nout++)
					u->n.stacknext = stack, stack = &u->n;
			}
	struct node **deps = malloc(sum * sizeof *deps);
	sum = 0;
	for (struct node *next, *node = stack; node; node = next) {
		next = node->stacknext;
		node->out = deps + sum;
		sum += node->nout;
	}
	for (struct dso *dso = dsos; dso < dsos + n; dso++)
		for (struct sym *y = dso->sym; y < dso->sym + dso->nsym; y++)
			if (y->weak != 'U' && y->vis != 'h') {
				struct sym *u;
				if (!(u=sym_htab_lookup_only(y->name))) continue;
				u->n.out++[0] = &y->n;
			}
	for (struct node *next, *node = stack; node; node = next) {
		next = node->stacknext; node->stacknext = 0;
		node->out -= node->nout;
	}
#if 0
	for (struct dso *dso = dsos; dso < dsos + n; dso++)
		for (struct sym *y = dso->sym; y < dso->sym + dso->nsym; y++)
			if (y->weak == 'U' && !y->n.nout) {
				printf("U %s\n", y->name);
			}
#endif
}

static void dfs(struct node *n)
{
	static int preorderidx;
	n->preorderidx = ++preorderidx;

	for (int i = 0; i < n->nout; i++) {
		struct node *o = n->out[i];
		if (!o->preorderidx)
			dfs(o);
	}
}
static void printsym(FILE *f, struct sym *sym)
{
	struct node *n = sym->defscn;
	struct obj *o = ((struct scn *)n)->objptr, *osrcid = o->srcidmain;
	const char *t, *objname = o->path;
	if ((t = strrchr(objname, '/'))) objname = t+1;
	char tls = sym->tls == 'T' ? 'T' : '_';
	fprintf(f, "%s:%s:%d:%c:%s\n",
		objname, o->srcid, osrcid->srcidcnt, tls, sym->name);
}
static void printundefsym(FILE *f, struct sym *sym)
{
	char tls = sym->tls == 'T' ? 'T' : '_';
	fprintf(f, "%s:%s:%d:%c:%s\n", "-", "f", -1, tls, sym->name);
}
static void mark(struct dso *dsos, int n)
{
	for (struct dso *dso = dsos; dso < dsos + n; dso++) {
		if (dso->entrysym)
			dfs(&dso->entrysym->n);
		for (struct scn *s = dso->scn; s < dso->scn + dso->nscn; s++)
			if (s->used && !s->n.preorderidx)
				dfs(&s->n);
	}
	for (struct dso *dso = dsos; dso < dsos + n; dso++) {
		if (dso->is_dso == 'R')
			continue;
		struct mark *mark = dso->mark = calloc (1, sizeof *dso->mark);
		for (struct sym *y = dso->sym; y < dso->sym + dso->nsym; y++) {
			if (y->weak == 'C') continue;
			if (y->weak == 'U') {
				struct sym *u = sym_htab_lookup_only(y->name);
				assert(u);
				if (!y->n.preorderidx) {
					if (/* ld_is_gold || */ !u->n.preorderidx) {
					    mark->nundef++;
					    y->n.stacknext = &mark->undefs->n;
					    mark->undefs = y;
					}
				} else {
					assert(u->n.preorderidx);
				}
			} else if (!y->n.preorderidx) {
				if (!y->defscn->preorderidx) {
					mark->nelim++;
					y->n.stacknext = &mark->elims->n;
					mark->elims = y;
				} else {
					mark->nloc++;
					y->n.stacknext = &mark->locs->n;
					mark->locs = y;
				}
			} else if (y->vis != 'h') {
				struct sym *u = sym_htab_lookup_only(y->name);
				if (!u || !u->n.preorderidx) {
					mark->nhid++;
					y->n.stacknext = &mark->hids->n;
					mark->hids = y;
				}
			}
		}
	}
}
void printmark(FILE *f, struct dso *dso)
{
	struct mark *mark = dso->mark;
        if (!mark)
		return;
	fprintf(f, "%d %d %d %d %s\n",
		mark->nelim, mark->nloc, mark->nhid, mark->nundef,
		dso->linkid);

	for (struct sym *y = mark->elims; y; y = (void*)y->n.stacknext)
		printsym(f, y);
	fprintf(f, "\n");

	for (struct sym *y = mark->locs; y; y = (void*)y->n.stacknext)
		printsym(f, y);
	fprintf(f, "\n");

	for (struct sym *y = mark->hids; y; y = (void*)y->n.stacknext)
		printsym(f, y);
	fprintf(f, "\n");

	for (struct sym *y = mark->undefs; y; y = (void*)y->n.stacknext)
		printundefsym(f, y);
	fprintf(f, "\n");
}
void merge(struct dso *dsos, int n)
{
	link(dsos, n);
	mark(dsos, n);
}

