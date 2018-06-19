// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/auxv.h>

#include "wrapper-common.h"

#define ORIG_CMD_SUFFIX "-real"
#define LIBMKPRIV "libmkpriv"
#define LIBDLSYM "libplug"
#define SIGNDLSYM "dlsym-signs"
#define DLSYMPLUG       PLUGDIR LIBDLSYM ".so"
#define MKPRIVPLUG      PLUGDIR LIBMKPRIV ".so"
#define IGNORE_PLUG_OPT	AUXDIR "ignored-for-plugopt"
#define DLSYMIN         AUXDIR SIGNDLSYM ".txt"

#if (!(GCC_RUN >= 0 || GCC_RUN <= 2))
#error "Compile gcc-wrapper with -DGCC_RUN={0,1,2}"
#endif


static const char *maybe_strip_lto(const char *opt)
{
	if (!strcmp(opt, "-flto") || !strncmp(opt, "-flto=", strlen("-flto=")))
		return "-fno-lto";
	return opt;
}

int main(int argc, char *argv[])
{
#if GCC_RUN == 0
	int sockfd = daemon_connect (argc, argv, "PreCompiler"[0]);
#elif GCC_RUN == 1
	int sockfd = daemon_connect (argc, argv, "Compiler"[0]);
#else
	int sockfd = daemon_connect(argc, argv, "SymbolHider"[0]);
#endif
	/* Need to signal that GCC keeps a socket descriptor. */
	char socket[12];
	sprintf(socket, "%d", sockfd);
	setenv(GCC_SOCKFD, socket, 1);

	char *fullname = (char *)getauxval(AT_EXECFN);
	char origcmd[strlen (fullname) + sizeof ORIG_CMD_SUFFIX];
	strcpy(origcmd, fullname);
	strcat(origcmd, ORIG_CMD_SUFFIX);

	const char *newargv[argc + 7 + 1];
	int newargc = 0;
	for (int i = 0; i < argc; i++)
		newargv[newargc++] = maybe_strip_lto(argv[i]);

	char optstr[64];
#if GCC_RUN == 0
	snprintf(optstr, sizeof optstr, "-fplugin-arg-" LIBDLSYM "-jfout=%d", sockfd);
	newargv[newargc++] = "-fplugin=" DLSYMPLUG;
	newargv[newargc++] = "-fplugin-arg-" LIBDLSYM "-run=0";
	newargv[newargc++] = optstr;
#elif GCC_RUN == 1
	char jfoptstr[64];
	snprintf(optstr, sizeof optstr, "-fplugin-arg-" LIBDLSYM "-symout=%d", sockfd);
	snprintf(jfoptstr, sizeof jfoptstr, "-fplugin-arg-" LIBDLSYM "-signs-fd=%d", sockfd);
	newargv[newargc++] = "-fplugin=" DLSYMPLUG;
	newargv[newargc++] = jfoptstr;
	newargv[newargc++] = optstr;
	newargv[newargc++] = "-fplugin=" MKPRIVPLUG;
	newargv[newargc++] = "-fplugin-arg-" LIBMKPRIV "-run=1";
#else
	newargv[newargc++] = "-fplugin=" MKPRIVPLUG;
	if (access(IGNORE_PLUG_OPT, F_OK)) {
		snprintf(optstr, sizeof optstr, "-fplugin-arg-" LIBMKPRIV "-sockfd=%d", sockfd);
		newargv[newargc++] = "-fplugin-arg-" LIBMKPRIV "-run=2";
		newargv[newargc++] = optstr;
	} else {
		newargv[newargc++] = "-fplugin-arg-" LIBMKPRIV "-run=1";
	}
#endif
#if GCC_RUN < 2 || SECTIONS
	newargv[newargc++] = "-ffunction-sections";
	newargv[newargc++] = "-fdata-sections";
#endif
	newargv[newargc++] = 0;
	execv(origcmd, (char **)newargv);
	die("gcc-wrapper: failed to execv %s: %s\n", origcmd, strerror(errno));
}
