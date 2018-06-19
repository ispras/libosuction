// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "daemon-ports.h"
#include "jfuncs.h"
#include "deps-graph.h"

#define STRING_(n) #n
#define STRING(n) STRING_(n)

enum visibility { ELIMINABLE, LOCAL, HIDDEN };

struct gccsym {
	int cnt;
	enum visibility vis;
	struct sym *sym;
	struct gccsym *next;
};

struct dso_entry {
	struct dso *dso;
	int ndups;
	struct dsolist {
		struct dso *value;
		struct dsolist *next;
	} *dups;
};

static int hidden_only = 0;
static struct jfnode *nodes = NULL;
static struct jfnode **jflist = &nodes;
static char *signatures;
static struct htab dsos_htab, gccsym_htab;
static int run1ready = 0;
static int run2ready = 0;

static struct dso_entry **dsos_htab_lookup(const char *linkid)
{
	if (dsos_htab.used * 3 >= dsos_htab.size)
		htab_expand(&dsos_htab);
	unsigned hash = sym_hash(linkid);
	for (size_t i = hash; ; i++) {
		i &= dsos_htab.size - 1;
		struct dso_entry **ptr = (void *)(dsos_htab.elts + i);
		if (!*ptr) {
			dsos_htab.hashes[i] = hash;
			return ptr;
		}
		if (dsos_htab.hashes[i] == hash
		    && !strcmp(linkid, (*ptr)->dso->linkid))
			return ptr;
	}
}

static struct dso_entry *dsos_htab_lookup_only(const char *linkid)
{
	if (!dsos_htab.used)
		return 0;
	unsigned hash = sym_hash(linkid);
	for (size_t i = hash; ; i++) {
		i &= dsos_htab.size - 1;
		struct dso_entry *dso_entry = dsos_htab.elts[i];
		if (!dso_entry)
			return 0;
		if (dsos_htab.hashes[i] == hash
		    && !strcmp(linkid, dso_entry->dso->linkid))
			return dso_entry;
	}
}

static void add_dso(FILE *f)
{
	struct dso *dso = calloc(1, sizeof (struct dso));
	if (input(dso, f))
		return;

	struct dso_entry **entry = dsos_htab_lookup(dso->linkid);
	if (*entry) {
		struct dsolist *dup = calloc(1, sizeof *dup);
		dup->value = dso;
		dup->next = (*entry)->dups;
		(*entry)->dups = dup;
		(*entry)->ndups++;
	} else {
		struct dso_entry *new_entry = calloc(1, sizeof *new_entry);
		new_entry->dso = dso;
		*entry = new_entry;
		dsos_htab.used++;
	}
}

static struct gccsym **gccsym_htab_lookup(struct sym *sym)
{
	if (gccsym_htab.used * 3 >= gccsym_htab.size)
		htab_expand(&gccsym_htab);
	struct obj *obj = ((struct scn *)sym->defscn)->objptr->srcidmain;
	unsigned hash = sym_hash(obj->srcid);
	for (size_t i = hash; ; i++) {
		i &= gccsym_htab.size - 1;
		struct gccsym **ptr = (void *)(gccsym_htab.elts + i);
		if (!*ptr) {
			gccsym_htab.hashes[i] = hash;
			return ptr;
		}
		struct obj *ptrobj = ((struct scn *)(*ptr)->sym->defscn)->objptr->srcidmain;
		if (gccsym_htab.hashes[i] == hash
		    && !strncmp(obj->srcid, ptrobj->srcid, 32))
			return ptr;
	}
}

static struct gccsym *gccsym_htab_lookup_only(const char *srcid)
{
	if (!gccsym_htab.used)
		return 0;
	unsigned hash = sym_hash(srcid);
	for (size_t i = hash; ; i++) {
		i &= gccsym_htab.size - 1;
		struct gccsym *gccsym = gccsym_htab.elts[i];
		if (!gccsym)
			return 0;
		struct obj *obj = ((struct scn *)gccsym->sym->defscn)->objptr->srcidmain;
		if (gccsym_htab.hashes[i] == hash
		    && !strncmp(srcid, obj->srcid, 32))
			return gccsym;
	}
}

static void add_gccsym(struct sym *sym, enum visibility vis)
{
	struct gccsym **gccsymp = gccsym_htab_lookup(sym);
	struct gccsym **prev = gccsymp, *curr = *gccsymp;
	if (hidden_only)
		vis = HIDDEN;
	while (curr) {
		if (!strcmp(curr->sym->name, sym->name)) {
			curr->cnt++;
			if (curr->vis < vis)
				curr->vis = vis;
			return;
		}
		prev = &curr->next;
		curr = curr->next;
	}
	struct gccsym *gccsym = calloc(1, sizeof *gccsym);
	gccsym->sym = sym;
	gccsym->vis = vis;
	gccsym->cnt = 1;
	*prev = gccsym;
	gccsym_htab.used++;
}

static void gc_gccsym()
{
	for (int i = 0; i < gccsym_htab.size; i++) {
		if (!gccsym_htab.elts[i])
			continue;
		struct gccsym **pp = (struct gccsym**)(gccsym_htab.elts + i);
		struct gccsym *entry = gccsym_htab.elts[i];
		struct obj *obj = ((struct scn *)entry->sym->defscn)->objptr->srcidmain;
		while(entry) {
			if (obj->srcidcnt == entry->cnt) {
				pp = &entry->next;
				entry = entry->next;
				continue;
			}
			assert(entry->cnt < obj->srcidcnt);

			*pp = entry->next;
			free(entry);
			entry = *pp;
		}
	}
}

static void amend_output(struct dso *dso)
{
	struct mark *mark = dso->mark;
	if (!mark)
		return;

	for (struct sym *y = mark->elims; y; y = (void*)y->n.stacknext)
		add_gccsym(y, ELIMINABLE);
	for (struct sym *y = mark->locs; y; y = (void*)y->n.stacknext)
		add_gccsym(y, LOCAL);
	for (struct sym *y = mark->hids; y; y = (void*)y->n.stacknext)
		add_gccsym(y, HIDDEN);
}

static void printgccsym(FILE *f, struct gccsym* gccsym)
{
	if (!gccsym) {
		fprintf(f, "0 0 0\n\n\n");
		return;
	}
	struct obj *obj = ((struct scn *)gccsym->sym->defscn)->objptr;
	const char *t, *objname = obj->path;
	if ((t = strrchr(objname, '/'))) objname = t+1;
	/* Count total symbols */
	int nelim = 0, nloc = 0, nhid = 0;
	struct gccsym* iter = gccsym;
	while(iter) {
		switch (iter->vis) {
			case ELIMINABLE:
				++nelim;
				break;
			case LOCAL:
				++nloc;
				break;
			case HIDDEN:
				++nhid;
				break;
		}
		iter = iter->next;
	}
	/* Order symbols for output */
	struct gccsym *order[nelim + nloc + nhid];
	int i = 0;
	int j = nelim;
	int k = nelim + nloc;
	iter = gccsym;
	while (iter) {
		switch (iter->vis) {
			case ELIMINABLE:
				order[i++] = iter;
				break;
			case LOCAL:
				order[j++] = iter;
				break;
			case HIDDEN:
				order[k++] = iter;
				break;
		}
		iter = iter->next;
	}

	fprintf(f, "%d %d %d\n", nelim, nloc, nhid);
	for (i = 0; i < nelim; ++i)
		fprintf(f, "%s:%s:%s\n", objname, obj->srcid, order[i]->sym->name);
	fprintf(f, "\n");
	for (; i < nelim + nloc; ++i)
		fprintf(f, "%s:%s:%s\n", objname, obj->srcid, order[i]->sym->name);
	fprintf(f, "\n");
	for (; i < nelim + nloc + nhid; ++i)
		fprintf(f, "%s:%s:%s\n", objname, obj->srcid, order[i]->sym->name);
}

static void prepare_run1(struct jfnode **list, const char *basename)
{
	if (run1ready)
		return;

	FILE *fbase = fopen(basename, "r");
	struct jfnode *dlbase = read_base(fbase);
	fclose(fbase);

	struct jfnode *closure = find_closure(dlbase, *list);

	int n = 0;
	for (struct jfnode *i = closure; i; i = i->next)
		++n;

	size_t size;
	FILE *out = open_memstream(&signatures, &size);
	fprintf(out, "%d\n", n);
	print_jflist(out, closure);
	fprintf(out, "%c", '\0');
	fclose(out);

	FILE *dump = fopen("dlsym-signs.txt", "w");
	fprintf(dump, "%s", signatures);
	fclose(dump);

	free_jflist(list);
	free_jflist(&closure);
	run1ready = 1;
}

static void prepare_run2(char **forces, int nforce)
{
	if (run2ready)
		return;

	int ndso = dsos_htab.used + nforce;
	for (int i = 0; i < dsos_htab.size; i++) {
		if (dsos_htab.elts[i]) {
			struct dso_entry *entry = dsos_htab.elts[i];
			ndso += entry->ndups;
		}
	}
	struct dso dsos[ndso];
	int dsoind;
	/* Add force-deps files */
	for (dsoind = 0; dsoind < nforce; dsoind++) {
		FILE *force = fopen(forces[dsoind], "r");
		int res = input(dsos + dsoind, force);
		if (res) fprintf (stderr, "Incorrect force file");
	}
	/* Add dsos from hash table */
	for (int i = 0, j = dsoind; i < dsos_htab.size; i++) {
		if (dsos_htab.elts[i]) {
			struct dso_entry *entry = dsos_htab.elts[i];
			assert (j < ndso);
			dsos[j++] = *entry->dso;
			struct dsolist *iter = entry->dups;
			while (iter) {
				assert (j < ndso);
				dsos[j++] = *iter->value;
				iter = iter->next;
			}
		}
	}
	/* Merge utility */
	merge(dsos, ndso);
	FILE *output = fopen("merged.vis", "w");
	for (struct dso *dso = dsos + nforce; dso < dsos + ndso; dso++)
		printmark(output, dso);
	fclose(output);
	/* Update hash table */
	for (int i = 0, j = dsoind; i < dsos_htab.size; i++) {
		if (dsos_htab.elts[i]) {
			struct dso_entry *entry = dsos_htab.elts[i];
			entry->dso->mark = dsos[j++].mark;
			struct dsolist *iter = entry->dups;
			while (iter) {
				iter->value->mark = dsos[j++].mark;
				iter = iter->next;
			}
		}
	}
	/* Amend deps-graph output for gcc plugin */
	for (struct dso *dso = dsos + nforce; dso < dsos + ndso; dso++) {
		amend_output(dso);
	}
	gc_gccsym();
	output = fopen("merged.vis.gcc", "w");
	for (int i = 0; i < gccsym_htab.size; i++)
		printgccsym(output, gccsym_htab.elts[i]);
	fclose(output);

	run2ready = 1;
}

static char *readfile(FILE *f, size_t *size)
{
	char *streamptr, c;
	FILE *out = open_memstream (&streamptr, size);
	while ((c = getc(f)) != EOF)
		putc(c, out);
	fclose(out);
	return streamptr;
}

static void writefile(FILE *f, const char *name)
{
	FILE *out = NULL;
	char buf[4096];
	size_t len;
	while ((len = fread(buf, 1, sizeof buf, f)) > 0) {
		if (!out) out = fopen(name, "w");
		fwrite(buf, 1, len, out);
	}
	if (out) fclose(out);
}

static const struct option opts[] = {
	{ .name = "port", .has_arg = 1, .val = 'p' },
	{ .name = "dlsym-base", .has_arg = 1, .val = 'b' },
	{ .name = "jfunc-files", .has_arg = 0, .val = 'j' },
	{ .name = "deps-files", .has_arg = 0, .val = 'd' },
	{ .name = "dlsym-files", .has_arg = 0, .val = 'l' },
	{ .name = "force-files", .has_arg = 0, .val = 'f' },
	{ .name = "hidden-only", .has_arg = 0, .val = 'h' },
	{ 0 }
};

static void usage(const char *name)
{
	fprintf(stderr, "usage: %s [--port n]\n", name);
}

int main(int argc, char *argv[])
{
	const char *port = STRING(DEFAULT_PORT);
	const char *dlsym_base = NULL;
	int v;
	int jfunc_start_idx, njfunc = 0;
	int deps_start_idx, ndeps = 0;
	int dlsym_start_idx, ndlsym = 0;
	int force_start_idx, nforce = 0;
	while ((v = getopt_long(argc, argv, "", opts, 0)) != -1)
		switch (v) {
		case 'p':
			port = optarg;
			break;
		case 'b':
			dlsym_base = optarg;
			break;
		case 'j':
			jfunc_start_idx = optind;
			while (optind < argc && argv[optind][0] != '-')
				++optind, ++njfunc;
			break;
		case 'd':
			deps_start_idx = optind;
			while (optind < argc && argv[optind][0] != '-')
				++optind, ++ndeps;
			break;
		case 'l':
			dlsym_start_idx = optind;
			while (optind < argc && argv[optind][0] != '-')
				++optind, ++ndlsym;
			break;
		case 'f':
			force_start_idx = optind;
			while (optind < argc && argv[optind][0] != '-')
				++optind, ++nforce;
			break;
		case 'h':
			hidden_only = 1;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	if (!port || optind < argc || !(dlsym_base || ndeps + ndlsym > 0)) {
		usage(argv[0]);
		return 1;
	}
	struct addrinfo *r, ai = {
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	int aierr = getaddrinfo(0, port, &ai, &r);
	if (aierr) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(aierr));
		return 1;
	}
	int sockfd, peerfd;
	if ((sockfd = socket(r->ai_family, r->ai_socktype, r->ai_protocol)) < 0
	    || bind(sockfd, r->ai_addr, r->ai_addrlen) < 0
	    || listen(sockfd, 128) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		return 1;
	}

	if (ndeps + ndlsym > 0) {
		fprintf(stderr, "Reading deps files %d\n", ndeps);
		for (int i = deps_start_idx; i < deps_start_idx + ndeps; ++i) {
			FILE *f = fopen(argv[i], "r");
			if (!f)
				fprintf(stderr, "deps file %s reading error\n", argv[i]);
			add_dso(f);
			fclose(f);
		}
		fprintf(stderr, "Reading dlsym files %d\n", ndlsym);
		for (int i = dlsym_start_idx; i < dlsym_start_idx + ndlsym; ++i) {
			FILE *f = fopen(argv[i], "r");
			if (!f)
				fprintf(stderr, "dlsym file %s reading error\n", argv[i]);
			input_dyndeps(f);
			fclose(f);
		}
		fprintf(stderr, "Started merging\n");
		prepare_run2(argv + force_start_idx, nforce);
		fprintf(stderr, "Finished merging\n");
	}
	if (njfunc > 0) {
		fprintf(stderr, "Reading jfunc files %d\n", njfunc);
		for (int i = jfunc_start_idx; i < jfunc_start_idx + njfunc; ++i) {
			FILE *f = fopen(argv[i], "r");
			if (!f)
				fprintf(stderr, "jfunc file %s reading error\n", argv[i]);
			read_jf(f, jflist);
			fclose(f);
		}
		fprintf(stderr, "Started transitive closure search\n");
		prepare_run1(jflist, dlsym_base);
		fprintf(stderr, "Finished transitive closure search\n");
	}

	setlinebuf(stdout);

	int fileno = 0;
#pragma omp parallel
#pragma omp master
	while ((peerfd = accept(sockfd, 0, 0)) >= 0)
#pragma omp task firstprivate(peerfd) shared(fileno, run1ready, run2ready, jflist)
	{
		int cmdlen;
		char *cmdline = 0;
		char tool = 0;
		FILE *fin, *fout;
		if (!(fin = fdopen(peerfd, "r"))
		    || fscanf(fin, "%c%d:", &tool, &cmdlen) != 2
		    || !(cmdline = malloc(cmdlen))
		    || fread(cmdline, cmdlen, 1, fin) != 1)
			fprintf(stderr, "%c read error\n", tool);
		else
			printf("%s\n", cmdline);
		free(cmdline);
		int fn = __sync_fetch_and_add(&fileno, 1);
		char namebuf[32];
		char md5hash[33];
		FILE *fcont;
		char *content;
		size_t contentsize;
		int n;
		switch (tool)
		  {
		  case 'L': /* Linker */
		    content = readfile (fin, &contentsize);

		    fcont = fmemopen(content, contentsize, "r");
		    snprintf(namebuf, sizeof namebuf, "deps-%03d", fn);
		    writefile (fcont, namebuf);
		    fclose(fcont);

		    fcont = fmemopen(content, contentsize, "r");
		    #pragma omp critical(merge)
		    add_dso(fcont);
		    fclose(fcont);

		    free(content);
		    break;
		  case 'P': /* PreCompiler */
		    content = readfile (fin, &contentsize);

		    fcont = fmemopen(content, contentsize, "r");
		    snprintf(namebuf, sizeof namebuf, "jfunc-%03d", fn);
		    writefile (fcont, namebuf);
		    fclose(fcont);

		    fcont = fmemopen(content, contentsize, "r");
		    #pragma omp critical(jfunc)
		    read_jf(fcont, jflist);
		    fclose(fcont);

		    free(content);
		    break;
		  case 'C': /* Compiler */
		    #pragma omp critical(jfunc)
		    prepare_run1(jflist, dlsym_base);

		    fcont = open_memstream (&content, &contentsize);
		    fout = fdopen(dup(peerfd), "w");

		    fprintf(fout, "%s", signatures);
		    fflush(fout);
		    while (fscanf(fin, "%d:", &n) == 1) {
			    char c;
			    for (int i = 0; i < n; ++i) {
				    while ((c = getc(fin)) != '\n')
					    putc(c, fcont);
				    putc(c, fcont);
			    }
			    fprintf(fout, "%s", signatures);
			    fflush(fout);
		    }
		    fclose(fcont);
		    fclose(fout);

		    fcont = fmemopen(content, contentsize, "r");
		    snprintf(namebuf, sizeof namebuf, "dlsym-%03d", fn);
		    writefile (fcont, namebuf);
		    fclose(fcont);

		    fcont = fmemopen(content, contentsize, "r");
		    #pragma omp critical(merge)
		    input_dyndeps(fcont);
		    fclose(fcont);

		    free(content);
		    break;
		  case 'E': /* Eliminator */
		    #pragma omp critical (merge)
		    prepare_run2(argv + force_start_idx, nforce);

		    if (fread(md5hash, 32, 1, fin) != 1) {
			    fprintf(stderr, "linkid read error\n");
			    break;
		    }
		    md5hash[32] = '\0';
		    struct dso_entry *entry = dsos_htab_lookup_only(md5hash);
		    if (entry) {
			    fout = fdopen(dup(peerfd), "w");
			    printmark(fout, entry->dso);
			    fclose(fout);
		    }
		    break;
		  case 'S': /* Symbol Hider */
		    #pragma omp critical (merge)
		    prepare_run2(argv + force_start_idx, nforce);
		    md5hash[32] = '\0';
		    while (fread(md5hash, 32, 1, fin) == 1) {
			    fout = fdopen(dup(peerfd), "w");
			    struct gccsym *gccsym = gccsym_htab_lookup_only(md5hash);
			    printgccsym(fout, gccsym);
			    fclose(fout);
		    }
		    break;
		  default:
		    fprintf(stderr, "Unknown tool: %c\n", tool);
		    snprintf(namebuf, sizeof namebuf, "unknown-%03d", fn);
		    writefile (fin, namebuf);
		    break;
		  }
		fclose(fin);
	}
}
