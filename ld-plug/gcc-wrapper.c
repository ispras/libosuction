#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "wrapper-common.h"

#define ORIG_CMD_SUFFIX "-real"
#define LIBMKPRIV "libmkpriv"
#define LIBDLSYM "libplug"
#define DLSYMPLUG "/tmp/gccwrap/" LIBDLSYM ".so"
#define MKPRIVPLUG "/tmp/gccwrap/" LIBMKPRIV ".so"
#define MERGED_PRIVDATA "/tmp/ldwrap/merged"

#if (!(GCC_RUN == 1 || GCC_RUN == 2))
#error "Compile gcc-wrapper with -DGCC_RUN={1,2}"
#endif


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
#if GCC_RUN == 1
	int sockfd = daemon_connect (argc, argv, "Compiler"[0]);
#endif

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
