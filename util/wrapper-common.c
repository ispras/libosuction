#include "wrapper-common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

void die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	exit(1);
}

int daemon_connect(int argc, char *argv[], char tool)
{
	int sockfd;
	struct sockaddr_un sa = {.sun_family = AF_UNIX};
	memcpy(sa.sun_path, "\0ldprivd", 8);

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0
	    || connect(sockfd, (void *) &sa, sizeof sa) < 0)
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
