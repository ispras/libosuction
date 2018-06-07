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
#include "deps-graph.h"

#define STRING_(n) #n
#define STRING(n) STRING_(n)

static struct htab dsos_htab, gccsym_htab;
static int run2ready = 0;

static struct dso **dsos_htab_lookup(const char *linkid)
{
	if (dsos_htab.used * 3 >= dsos_htab.size)
		htab_expand(&dsos_htab);
	unsigned hash = sym_hash(linkid);
	for (size_t i = hash; ; i++) {
		i &= dsos_htab.size - 1;
		struct dso **ptr = (void *)(dsos_htab.elts + i);
		if (!*ptr) {
			dsos_htab.hashes[i] = hash;
			return ptr;
		}
		if (dsos_htab.hashes[i] == hash
		    && !strcmp(linkid, (*ptr)->linkid))
			return ptr;
	}
}

static struct dso *dsos_htab_lookup_only(const char *linkid)
{
	if (!dsos_htab.used)
		return 0;
	unsigned hash = sym_hash(linkid);
	for (size_t i = hash; ; i++) {
		i &= dsos_htab.size - 1;
		struct dso *dso = dsos_htab.elts[i];
		if (!dso)
			return 0;
		if (dsos_htab.hashes[i] == hash
		    && !strcmp(linkid, dso->linkid))
			return dso;
	}
}

static void prepare_run2(char **forces, int nforce)
{
	if (run2ready)
		return;

	int ndso = dsos_htab.used + nforce;
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
			struct dso *dso = dsos_htab.elts[i];
			assert (j < ndso);
			dsos[j++] = *dso;
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
			struct dso *dso = dsos_htab.elts[i];
			dso->mark = dsos[j++].mark;
		}
	}
	run2ready = 1;
}

static void add_dso(FILE *f)
{
	struct dso *deps = calloc(1, sizeof (struct dso));
	if (input(deps, f))
		return;

	struct dso **depsp = dsos_htab_lookup(deps->linkid);
	if (*depsp) {
		free (deps);
	} else {
		*depsp = deps;
		dsos_htab.used++;
	}
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
	{ .name = "deps-files", .has_arg = 0, .val = 'd' },
	{ .name = "dlsym-files", .has_arg = 0, .val = 'l' },
	{ .name = "force-files", .has_arg = 0, .val = 'f' },
	{ 0 }
};

static void usage(const char *name)
{
	fprintf(stderr, "usage: %s [--port n]\n", name);
}

int main(int argc, char *argv[])
{
	const char *port = STRING(DEFAULT_PORT);
	int v;
	int deps_start_idx, ndeps = 0;
	int dlsym_start_idx, ndlsym = 0;
	int force_start_idx, nforce = 0;
	while ((v = getopt_long(argc, argv, "", opts, 0)) != -1)
		switch (v) {
		case 'p':
			port = optarg;
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
		default:
			usage(argv[0]);
			return 1;
		}
	if (!port || optind < argc) {
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

	setlinebuf(stdout);

	int fileno = 0;
#pragma omp parallel
#pragma omp master
	while ((peerfd = accept(sockfd, 0, 0)) >= 0)
#pragma omp task firstprivate(peerfd) shared(fileno)
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
		    snprintf(namebuf, sizeof namebuf, "jfunc-%03d", fn);
		    writefile (fin, namebuf);
		    break;
		  case 'C': /* Compiler */
		    content = readfile (fin, &contentsize);

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

		    if (fread(md5hash, 32, 1, fin) != 1)
			    fprintf(stderr, "linkid read error\n");
		    md5hash[32] = '\0';
		    struct dso *dso = dsos_htab_lookup_only(md5hash);
		    if (dso) {
			    fout = fdopen(dup(peerfd), "w");
			    printmark(fout, dso);
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
