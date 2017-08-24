#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <elf.h>

#include "wrapper-common.h"
#include "ld-plug/md5.h"
#include "ld-plug/elf-common.h"

#define ALTDIR (PLUGDIR "ld/")
#define PLUGIN (PLUGDIR "ld/libplug.so")
#define PLUGIN_PRIV (PLUGDIR "ld/libplug-priv.so")

#if (!(GCC_RUN == 1 || GCC_RUN == 2))
#error "Compile ld wrapper with -DGCC_RUN={1,2}"
#endif

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
	if (fd = open(filename, O_RDONLY) < 0)
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

static void gen_linkid(char *md5out, int argc, char *argv[])
{
	unsigned char md5sum[16], md5all[16] = { 0 };
	for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		size_t l = strlen(arg);
		if (l < 2 || arg[l-1] != 'o') continue;
		if (l == 2 && arg[0] == '-') { i++; continue; }
		if (arg[l-2] != '.') continue;
		if (get_srcid(md5sum, arg))
			for (int j = 0; j < sizeof md5sum; j++)
				md5all[j] ^= md5sum[j];
	}
	printmd5(md5out, md5all);
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

	char origcmd[7 + sizeof ALTDIR] = ALTDIR;
	strcpy(origcmd + sizeof ALTDIR - 1, name);

	char *newargv[argc + 6];
	memcpy(newargv, argv, argc * sizeof *argv);

	char optstr[128];
	gen_linkid(optstr, argc, argv);
#if GCC_RUN == 1
	int sockfd = daemon_connect(argc, argv, "Linker"[0]);

	char *entry = "_start";
	for (int i = 1; i < argc - 1; i++)
		if (!strcmp(argv[i], "-e")) {
			entry = argv[i+1];
			break;
		}
	snprintf(optstr + 32, sizeof optstr - 32, ":%s:%d", entry, sockfd);
	newargv[argc++] = "--plugin";
	newargv[argc++] = PLUGIN;
#else
	newargv[argc++] = "--gc-sections";
	newargv[argc++] = "--plugin";
	newargv[argc++] = PLUGIN_PRIV;
	snprintf(optstr + 32, sizeof optstr - 32, ":%s", MERGED_PRIVDATA);
#endif
	newargv[argc++] = "--plugin-opt";
	newargv[argc++] = optstr;
	newargv[argc++] = 0;
	execv(origcmd, newargv);
	die("execve: %s\n", strerror(errno));
}
