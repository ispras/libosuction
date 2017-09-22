#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <elf.h>

#include "wrapper-common.h"
#include "ld-plug/md5.h"
#include "ld-plug/elf-common.h"

#define ALTDIR (PLUGDIR "ld/")
#define PLUGIN (PLUGDIR "ld/libplug.so")
#define PLUGIN_PRIV (PLUGDIR "ld/libplug-priv.so")
#define DUMMY_OBJ (PLUGDIR "ld/dummy.o")

#if (!(GCC_RUN == 1 || GCC_RUN == 2))
#error "Compile ld wrapper with -DGCC_RUN={1,2}"
#endif

#define SRCID_FILE PLUGDIR "srcid.o"

static int get_elfnote_srcid(unsigned char *md5sum, const unsigned char *view)
{
	const ElfNN_(Ehdr) *ehdr = (void *)view;
	typedef ElfNN_(Shdr) Shdr;
	if (memcmp (ehdr->e_ident, ELFMAG, SELFMAG))
		return 0;
	if (ehdr->e_ident[4] != ELFCLASS)
		return 0;
	if (ehdr->e_ident[5] != ELFDATA)
		return 0;
	if (ehdr->e_type != ET_REL)
		return 0;
	Shdr *shdrs = (void *)(view + ehdr->e_shoff);
	if (ehdr->e_shentsize != sizeof *shdrs)
		return 0;
	int shnum = ehdr->e_shnum;
	if (!shnum && ehdr->e_shoff)
		shnum = shdrs[0].sh_size;
	if (shnum < 2) return 0;
	int shstrndx = ehdr->e_shstrndx;
	if (shstrndx == SHN_XINDEX)
		shstrndx = shdrs[0].sh_link;
	const char *shstrtab = (void *)(view + shdrs[shstrndx].sh_offset);
	const char *srcid = 0;
	for (int i = 1; i < shnum; i++) {
		Shdr *shdr = shdrs + i;
		if (shdr->sh_type == SHT_NOTE
		    && shdr->sh_flags == SHF_EXCLUDE
		    && (srcid = plug_srcid(shstrtab + shdr->sh_name)))
			break;
	}
	if (!srcid) return 0;
	for (int i = 0; i < 16; i++, md5sum++, srcid += 2)
		sscanf(srcid, "%2hhx", md5sum);
	return 1;
}

static int get_srcid(unsigned char *md5sum, const char *filename)
{
	int fd, status = 0;
	if ((fd = open(filename, O_RDONLY)) < 0)
		return 0;
	struct stat s;
	if (fstat(fd, &s) < 0)
		goto done;
	void *view;
	if ((view = mmap(0, s.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED)
		goto done;
	if (!get_elfnote_srcid(md5sum, view))
		md5_buffer(view, s.st_size, md5sum);
	munmap(view, s.st_size);
	status = 1;
done:
	close(fd);
	return status;
}

static int gen_linkid(char *md5out, int argc, char *argv[])
{
	int nobjs = 0;
	unsigned char md5sum[16], md5all[16] = { 0 };
	for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		size_t l = strlen(arg);
		if (!strcmp(arg, "-o")) { i++; continue; }
		if (!(l >= 2 && !strcmp(".o", arg + l - 2)
		      || l >= 3 && (!strcmp(".os", arg + l - 3)
				    || !strcmp(".lo", arg + l - 3))))
		      continue;
		nobjs++;
		if (get_srcid(md5sum, arg))
			for (int j = 0; j < sizeof md5sum; j++)
				md5all[j] ^= md5sum[j];
	}
	if (nobjs == 1) md5all[15]++;
	printmd5(md5out, md5all);
	return nobjs;
}

static void parse_entry_out(int argc, char **argv, char **entry, char **outfile)
{
	static const char *eo[] = { "--entry", "--entr", "--ent", "--en",
				    "--output", "--outpu", "--outp", "--out", "--ou" };
	for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		int arglen = strlen(arg);

		if (!strncmp(arg, "-o", 2) || !strncmp(arg, "-e", 2)) {
			char **smth = arg[1] == 'o' ? outfile : entry;
			if (strlen (arg) == 2 && i < argc - 1)
				*smth = argv[i + 1];
			else
				*smth = arg + 2;
		}

		for (int j = 0; j < (int)(sizeof eo / sizeof *eo); j++) {
			int eojlen = strlen(eo[j]);
			char **smth = eo[j][2] == 'o' ? outfile : entry;
			if (!strncmp(arg, eo[j], eojlen)) {
				if (arglen == eojlen && i < argc - 1)
					*smth = argv[i + 1];
				else if (arg[eojlen] == '=')
					*smth = arg + eojlen;
			}
		}
	}
}

void exec_child(char *cmd, char **argv)
{
	pid_t cpid = fork();
	if (cpid == -1)
		die("ld-wrapper: fork: %s\n", strerror(errno));
	if (cpid == 0) {
		execv(cmd, argv);
		die("ld-wrapper: execve: %s\n", strerror(errno));
	}
	int status;
	pid_t w = waitpid(cpid, &status, 0);
	if (w == -1)
		die("ld-wrapper: waitpid: %s\n", strerror(errno));
	if (!WIFEXITED(status))
		die("ld-wrapper: child terminated abnormally");
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

	char *entry = "_start";
	char *outfile = "a.out";
	parse_entry_out(argc, argv, &entry, &outfile);

	char origcmd[7 + sizeof ALTDIR] = ALTDIR;
	strcpy(origcmd + sizeof ALTDIR - 1, name);

	char *storage[argc + 7];
	char **newargv = storage;

	bool incremental_p = false;

	char optstr[128];
	if (!gen_linkid(optstr, argc, argv)) {
		newargv = argv;
		goto exec;
	}

	for (int i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "-i")
		    || !strcmp(argv[i], "--relocatable")) {
			incremental_p = true;
			break;
		}

	/* Can't set --gc-sections with -r (without specifying a single root). */
	if (incremental_p && GCC_RUN == 2) {
		newargv = argv;
		goto exec;
	}

	newargv[0] = argv[0];
	memcpy(newargv + GCC_RUN, argv + 1, (argc - 1) * sizeof *argv);

#if GCC_RUN == 1
	int sockfd = daemon_connect(argc, argv, "Linker"[0]);
	snprintf(optstr + 32, sizeof optstr - 32, ":%s:%d", entry, sockfd);
	newargv[argc++] = "--plugin";
	newargv[argc++] = PLUGIN;
#else
	newargv[1] = DUMMY_OBJ;
	argc++;
	newargv[argc++] = "--gc-sections";
	newargv[argc++] = "--plugin";
	newargv[argc++] = PLUGIN_PRIV;
	snprintf(optstr + 32, sizeof optstr - 32, ":%s", MERGED_PRIVDATA);
#endif
	newargv[argc++] = "--plugin-opt";
	newargv[argc++] = optstr;
	newargv[argc++] = 0;
exec:;
	if (incremental_p) {
		exec_child(origcmd, newargv);
		char *strip_argv[] = { "strip",
				       "--remove-section=" PLUG_SECTION_PREFIX "*",
				       "--wildcard", "--keep-symbol=*",
				       outfile, NULL };
		exec_child("/usr/bin/strip", strip_argv);

		char tmpfile[] = "/tmp/ld-r-XXXXXX";
		int tmpfd = mkstemp(tmpfile);
		if (-1 == tmpfd) die("ld-wrapper: cannot create temp file");
		close(tmpfd);

		char *ld_r_argv[] = { origcmd,
				      "--relocatable", outfile, SRCID_FILE,
				      "-o", tmpfile, NULL };
		exec_child(origcmd, ld_r_argv);

		char rename_expr[] =
			PLUG_SECTION_PREFIX  "0123456789abcdef0123456789abcdef" "="
			PLUG_SECTION_PREFIX "\0_23456789abcdef0123456789abcdef";
		strncpy(rename_expr + strlen(rename_expr), optstr, 32);
		char *objcopy_arv[] = { "objcopy",
					"--rename-section", rename_expr,
					tmpfile, outfile, NULL };
		exec_child("/usr/bin/objcopy", objcopy_arv);
		unlink(tmpfile);
	} else {
		execv(origcmd, newargv);
		die("execve: %s\n", strerror(errno));
	}
	return 0;
}
