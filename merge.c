#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
	enum {N_SCN, N_SYM} kind;
	int nout;
	struct node **out;

	int preorderidx;
	int lowlink;
	int onstack;
	struct node *stacknext;
	struct node *sccroot;
	int sccsize;
};

struct dso {
	int is_dso;
	int nobj;
	int nscn;
	int nsym;
	struct obj {
		const char *path;
		long long offset;
		int nscn;
		struct scn *scns;
	} *obj;
	struct scn {
		struct node n;
		int used;
		int nscndeps;
		int nsymdeps;
		long long size;
		const char *name;
		struct scn **scndeps;
		struct sym **symdeps;
	} *scn;
        struct sym {
		struct node n;
		char weak;
		char vis;
		int nrevdeps;
		const char *name;
		struct node *defscn;
		struct scn **revdeps;
	} *sym;
};

static struct htab {
	unsigned *hashes;
	void **elts;
	size_t size;
	size_t used;
} objs_htab, syms_htab;

static inline unsigned
sym_hash(const char *name)
{
	const unsigned char *c = (const void *)name;
	unsigned h = 5381;
	for (; *c; c++)
		h += h*32 + *c;
	return h;
}
static inline unsigned
obj_hash(const char *path, long long offset)
{
	const unsigned char *c = (const void *)path;
	unsigned h = 5381;
	for (; *c; c++)
		h += h*32 + *c;
	h ^= offset;
	return h;
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
static void
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
obj_htab_lookup(const char *path, long long offset)
{
	if (objs_htab.used * 3 >= objs_htab.size)
		htab_expand(&objs_htab);
	unsigned hash = obj_hash(path, offset);
	for (size_t i = hash; ; i++) {
		i &= syms_htab.size - 1;
		struct obj **ptr = (void *)(syms_htab.elts + i);
		if (!*ptr) {
			syms_htab.hashes[i] = hash;
			return ptr;
		}
		if (syms_htab.hashes[i] == hash
		    && offset == (*ptr)->offset
		    && !strcmp(path, (*ptr)->path))
			return ptr;
	}
}
static void
input(struct dso *dso, FILE *f)
{
	char is_dso;
	fscanf(f, " %c %d %d", &is_dso, &dso->nobj, &dso->nscn);
	dso->is_dso = is_dso == 'D';
	struct obj *o = dso->obj = calloc(dso->nobj, sizeof *dso->obj);
	struct scn *s = dso->scn = calloc(dso->nscn, sizeof *dso->scn);
	for (; o < dso->obj + dso->nobj; o++) {
		fscanf(f, "%d %lld %ms", &o->nscn, &o->offset, &o->path);
		o->scns = s;
		for (; s < o->scns + o->nscn; s++) {
			fscanf(f, "%d %lld %ms %*[^\n]", &s->used, &s->size, &s->name);
			fscanf(f, "%d", &s->nscndeps);
			if (!s->nscndeps) continue;
			s->scndeps = malloc(s->nscndeps * sizeof *s->scndeps);
			for (int i = 0; i < s->nscndeps; i++) {
				int t;
				fscanf(f, "%d", &t);
				s->scndeps[i] = dso->scn + t;
			}
		}
	}
	fscanf(f, "%d", &dso->nsym);
	struct sym *y = dso->sym = calloc(dso->nsym, sizeof *dso->sym);
	for (; y < dso->sym + dso->nsym; y++) {
		int t;
		fscanf(f, " %c%c %d %ms", &y->weak, &y->vis, &t, &y->name);
		y->n.kind = N_SYM;
		if (t >= 0) {
			y->defscn = &dso->scn[t].n;
			y->n.nout = 1;
			y->n.out = &y->defscn;
		}
		fscanf(f, "%d", &y->nrevdeps);
		if (!y->nrevdeps) continue;
		y->revdeps = malloc(y->nrevdeps * sizeof *y->revdeps);
		for (int i = 0; i < y->nrevdeps; i++) {
			int t;
			fscanf(f, "%d", &t);
			y->revdeps[i] = dso->scn + t;
			dso->scn[t].nsymdeps++;
		}
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

static void scc(struct dso *dso)
{
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
static void mark(struct dso *dsos, int n)
{
	for (struct dso *dso = dsos; dso < dsos + n; dso++)
		for (struct scn *s = dso->scn; s < dso->scn + dso->nscn; s++)
			if (s->used)
				dfs(&s->n);
	int nloc = 0, nhid = 0;
	struct sym *locstack = 0, *hidstack = 0;
	for (struct dso *dso = dsos; dso < dsos + n; dso++)
		for (struct sym *y = dso->sym; y < dso->sym + dso->nsym; y++) {
			if (y->weak == 'U') continue;
			if (!y->n.preorderidx) {
				nloc++;
				y->n.stacknext = &locstack->n; locstack = y;
			} else if (y->vis != 'h') {
				struct sym *u = sym_htab_lookup_only(y->name);
				if (!u || !u->n.preorderidx) {
					nhid++;
					y->n.stacknext = &hidstack->n; hidstack = y;
				}
			}
		}
	printf("%d %d\n", nloc, nhid);
	for (; nloc; nloc--, locstack = (void *)locstack->n.stacknext)
		printf("%s\n", locstack->name);
	for (; nhid; nhid--, hidstack = (void *)hidstack->n.stacknext)
		printf("%s\n", hidstack->name);
}

int main(int argc, char *argv[])
{
	struct dso dsos[argc-1];
	for (int i = 1; i < argc; i++) {
		FILE *f = fopen(argv[i], "r");
		if (!f) return 1;
		input(dsos+i-1, f);
		fclose(f);
	}
	//scc(&dso1);
	link(dsos, argc-1);
	mark(dsos, argc-1);
}
