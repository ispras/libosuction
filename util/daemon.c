// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "daemon-ports.h"

#define STRING_(n) #n
#define STRING(n) STRING_(n)

static const struct option opts[] = {
	{ .name = "cc",   .has_arg = 0, .val = 'c' },
	{ .name = "ld",   .has_arg = 0, .val = 'l' },
	{ .name = "port", .has_arg = 1, .val = 'p' },
	{ 0 }
};

static void usage(const char *name)
{
	fprintf(stderr, "usage: %s (--cc | --ld) [--port n]\n", name);
}

int main(int argc, char *argv[])
{
	const char *port = 0;
	int v;
	while ((v = getopt_long(argc, argv, "", opts, 0)) != -1)
		switch (v) {
		case 'c':
			if (!port) port = STRING(DEFAULT_PORT_CC);
			break;
		case 'l':
			if (!port) port = STRING(DEFAULT_PORT_LD);
			break;
		case 'p':
			port = optarg;
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
		FILE *f;
		if (!(f = fdopen(peerfd, "r"))
		    || fscanf(f, "%c%d:", &tool, &cmdlen) != 2
		    || tool != 'L' && tool != 'C' && tool != 'P'
		    || !(cmdline = malloc(cmdlen))
		    || fread(cmdline, cmdlen, 1, f) != 1)
			fprintf(stderr, "%c read error\n", tool);
		else
			printf("%s\n", cmdline);
		free(cmdline);
		int fn = __sync_fetch_and_add(&fileno, 1);
		char namebuf[32];
		switch (tool)
		  {
		  case 'L':
		    snprintf(namebuf, sizeof namebuf, "deps-%03d", fn);
		    break;
		  case 'P':
		    snprintf(namebuf, sizeof namebuf, "jfunc-%03d", fn);
		    break;
		  case 'C':
		    snprintf(namebuf, sizeof namebuf, "dlsym-%03d", fn);
		    break;
		  default:
		    fprintf(stderr, "Unknown tool: %c\n", tool);
		    snprintf(namebuf, sizeof namebuf, "unknown-%03d", fn);
		    break;
		  }
		FILE *out = NULL;
		char buf[4096];
		size_t len;
		while ((len = fread(buf, 1, sizeof buf, f)) > 0) {
			if (!out) out = fopen(namebuf, "w");
			fwrite(buf, 1, len, out);
		}
		if (out) fclose(out);
		fclose(f);
	}
}
