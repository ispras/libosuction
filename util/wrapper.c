#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "wrapper-common.h"

#define ALTDIR (PLUGDIR "ld/")
#define PLUGIN (PLUGDIR "ld/libplug.so")
#define PLUGIN_PRIV (PLUGDIR "ld/libplug-priv.so")

#if (!(GCC_RUN == 1 || GCC_RUN == 2))
#error "Compile ld wrapper with -DGCC_RUN={1,2}"
#endif

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

	char origcmd[7 + sizeof ALTDIR] = ALTDIR;
	strcpy(origcmd + sizeof ALTDIR - 1, name);

	char *newargv[argc + 5];
	memcpy(newargv, argv, argc * sizeof *argv);
#if GCC_RUN == 1
	int sockfd = daemon_connect(argc, argv, "Linker"[0]);

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
#else
	newargv[argc++] = "--plugin";
	newargv[argc++] = PLUGIN_PRIV;
	newargv[argc++] = "--plugin-opt";
	newargv[argc++] = MERGED_PRIVDATA;
#endif
	newargv[argc++] = 0;
	execv(origcmd, newargv);
	die("execve: %s\n", strerror(errno));
}
