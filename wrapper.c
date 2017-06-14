#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#define ALTDIR "/tmp/ldwrap/"
#define PLUGIN "/tmp/ldwrap/libplug.so"

static void die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	exit(1);
}

int main(int argc, char *argv[])
{
	char *name = strrchr(argv[0], '/');
	if (!name) name = argv[0]; else name++;

	static const char oknames[][8] = {"ld", "ld.bfd", "ld.gold"};
	for (int i = 0; i < sizeof oknames / sizeof *oknames; i++)
		if (!strcmp(name, oknames[i]))
			goto ok;
	die("bad wrapped command\n");
ok:;

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
	    || fprintf(f, "%d:", cmdlen) < 0
	    || fwrite(cmdline, cmdlen, 1, f) != 1
	    || fflush(f) != 0)
		die("write error\n");

	char origcmd[7 + sizeof ALTDIR] = ALTDIR;
	strcpy(origcmd + sizeof ALTDIR - 1, name);

	char *newargv[argc + 5];
	memcpy(newargv, argv, argc * sizeof *argv);
	char optstr[64], *entry = "";
	for (int i = 1; i < argc - 1; i++)
		if (!strcmp(argv[i], "-e")) {
			entry = argv[i+1];
			break;
		}
	snprintf(optstr, sizeof optstr, "%d:%s", sockfd, entry);
	newargv[argc++] = "--plugin";
	newargv[argc++] = PLUGIN;
	newargv[argc++] = "--plugin-opt";
	newargv[argc++] = optstr;
	newargv[argc++] = 0;
	execv(origcmd, newargv);
	die("execve: %s\n", strerror(errno));
}
