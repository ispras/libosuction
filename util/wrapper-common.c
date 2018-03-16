// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "wrapper-common.h"
#include "daemon-ports.h"

#define STRING_(n) #n
#define STRING(n) STRING_(n)

void die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	exit(1);
}

static void read_config(const char **phost, const char **pport, char tool)
{
	int tool_is_ld = tool == 'L';
	const char *envname = tool_is_ld ? "MKPRIVD_LD" : "MKPRIVD_CC";
	const char *env = getenv(envname);
	if (env) {
		if (sscanf(env, "%m[^:]:%ms", phost, pport) != 2)
			die("%s: parse error\n", envname);
		return;
	}
	const char *path = tool_is_ld ? "/etc/mkprivd-ld" : "/etc/mkprivd-cc";
	FILE *f = fopen(path, "r");
	if (f) {
		if (fscanf(f, "%m[^:]:%ms", phost, pport) != 2)
			die("%s: parse error\n", path);
		fclose(f);
		return;
	}
	*phost = "localhost";
	*pport = tool_is_ld ? STRING(DEFAULT_PORT_LD) : STRING(DEFAULT_PORT_CC);
}

int daemon_connect(int argc, char *argv[], char tool)
{
	int sockfd;
	const char *host, *port;
	read_config(&host, &port, tool);
	struct addrinfo *r, ai = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	int aierr = getaddrinfo(host, port, &ai, &r);
	if (aierr)
		die("getaddrinfo: %s", gai_strerror(aierr));

	if ((sockfd = socket(r->ai_family, r->ai_socktype, r->ai_protocol)) < 0
	    || connect(sockfd, r->ai_addr, r->ai_addrlen) < 0)
		die("%s\n", strerror(errno));

	int cmdlen = 0;
	for (int i = 0; i < argc; i++)
		cmdlen += strlen(argv[i]) + 1;
	char *cmdline = malloc(cmdlen), *c = cmdline;
	for (int i = 0; i < argc; i++)
		(c = stpcpy(c, argv[i]) + 1)[-1] = ' ';
	c[-1] = 0;
	FILE *f;
	if (!(f = fdopen(sockfd, "w"))
	    || fprintf(f, "%c%d:", tool, cmdlen) < 0
	    || fwrite(cmdline, cmdlen, 1, f) != 1
	    || fflush(f) != 0)
		die("write error\n");
	return sockfd;
}
