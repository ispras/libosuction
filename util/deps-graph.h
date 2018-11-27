// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#ifndef DEPS_GRAPH_H
#define DEPS_GRAPH_H

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
	int weight;
};

struct dso {
	const char *name;
	const char *entrypoint;
	const char *linkid;
	int is_dso;
	int nobj;
	int nscn;
	int nsym;
	struct obj {
		const char *path;
		const char *srcid;
		long long offset;
		struct obj *srcidmain;
		int srcidcnt;
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
		struct obj *objptr;
		struct scn **scndeps;
		struct sym **symdeps;
	} *scn;
        struct sym {
		struct node n;
		char weak;
		char vis;
		char tls;
		int nrevdeps;
		const char *name;
		struct node *defscn;
		struct scn **revdeps;
	} *sym, *entrysym;
	struct mark {
		int nelim;
		int nloc;
		int nhid;
		int nundef;
		struct sym *elims;
		struct sym *locs;
		struct sym *hids;
		struct sym *undefs;
	} *mark;
};

struct htab {
	unsigned *hashes;
	void **elts;
	size_t size;
	size_t used;
};

unsigned sym_hash(const char *name);
void htab_expand(struct htab *htab);
void input_dyndeps(FILE *f);
int input(struct dso *dso, FILE *f);
void merge(struct dso *dsos, int n);
void printmark(FILE *f, struct dso *dso);

#endif  /* deps-graph.h */

