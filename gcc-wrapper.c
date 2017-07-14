#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#define ORIG_CMD_SUFFIX "-real"
#define LIBMKPRIV "libmkpriv"
#define LIBDLSYM "libplug"
#define DLSYMPLUG "/tmp/gccwrap/" LIBDLSYM ".so"
#define MKPRIVPLUG "/tmp/gccwrap/" LIBMKPRIV ".so"
#define MERGED_PRIVDATA "/tmp/ldwrap/merged"

#if (!(GCC_RUN == 1 || GCC_RUN == 2))
#error "Compile gcc-wrapper with -DGCC_RUN={1,2}"
#endif

static void die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	exit(1);
}

static const char *maybe_strip_lto(const char *opt)
{
#if GCC_RUN == 1
	if (!strcmp(opt, "-flto") || !strncmp(opt, "-flto=", strlen("-flto=")))
		return "-fno-lto";
#endif
	return opt;
}

int main(int argc, char *argv[])
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
	    || fprintf(f, "C%d:", cmdlen) < 0
	    || fwrite(cmdline, cmdlen, 1, f) != 1
	    || fflush(f) != 0)
		die("write error\n");

	/* TODO: refactor out the common part ^ */

	char origcmd[strlen (argv[0]) + sizeof ORIG_CMD_SUFFIX];
	strcpy(origcmd, argv[0]);
	strcat(origcmd, ORIG_CMD_SUFFIX);

	const char *newargv[argc + 4 + 1];
	int newargc = 0;
	for (int i = 0; i < argc; i++)
		newargv[newargc++] = maybe_strip_lto(argv[i]);

#if GCC_RUN == 1
	char optstr[64];
	snprintf(optstr, sizeof optstr, "-fplugin-arg-" LIBDLSYM "-out=%d", sockfd);
	newargv[newargc++] = "-fplugin=" DLSYMPLUG;
	newargv[newargc++] = optstr;
	newargv[newargc++] = "-fplugin=" MKPRIVPLUG;
	newargv[newargc++] = "-fplugin-arg-" LIBMKPRIV "-run=1";
#else
	newargv[newargc++] = "-fplugin=" MKPRIVPLUG;
	newargv[newargc++] = "-fplugin-arg-" LIBMKPRIV "-run=2";
	newargv[newargc++] = "-fplugin-arg-" LIBMKPRIV "-fname=" MERGED_PRIVDATA;
#endif
	newargv[newargc++] = 0;
	execv(origcmd, (char **)newargv);
	die("execve: %s\n", strerror(errno));
}
