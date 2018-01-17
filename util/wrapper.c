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

#include <ar.h>
#include <elf.h>

#include "wrapper-common.h"
#include "ld-plug/md5.h"
#include "ld-plug/elf-common.h"

#define ALTDIR (PLUGDIR "ld/")
#define PLUGIN (PLUGDIR "ld/libplug.so")
#define PLUGIN_PRIV (PLUGDIR "ld/libplug-priv.so")
#define DUMMY_OBJ (PLUGDIR "ld/dummy.o")
#define SRCID_FILE (PLUGDIR "ld/srcid.o")

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

static int get_file_srcid(unsigned char *md5sum,
			  const unsigned char *view,
			  off_t size)
{
	if (!get_elfnote_srcid(md5sum, view))
		md5_buffer(view, size, md5sum);
	return 1;
}

static int get_ar_srcid(unsigned char *md5sum,
			const unsigned char *view,
			off_t size)
{
	unsigned char md5part[16];
	const unsigned char *view0 = view;
	if (size < SARMAG || memcmp(view, ARMAG, SARMAG))
		return 0;
	view += SARMAG;

	struct ar_hdr *hdr;
	long long member_size;
	memset(md5sum, 0, 16);
	while (view + sizeof *hdr <= view0 + size) {
		hdr = (void *)view;
		view += sizeof *hdr;
		sscanf(hdr->ar_size, "%lld", &member_size);
		if (hdr->ar_name[0] != '/') {
		    get_file_srcid(md5part, view, member_size);
		    for (int j = 0; j < sizeof md5part; j++)
			    md5sum[j] ^= md5part[j];
		}
		view += member_size + (member_size & 1);
	}
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
	int len = strlen(filename);
	if (len >= 2 && !strncmp(filename + len - 2, ".a", 2))
		status = get_ar_srcid(md5sum, view, s.st_size);
	else
		status = get_file_srcid(md5sum, view, s.st_size);
	munmap(view, s.st_size);
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
		if (!strcmp(arg, "-lgcc_eh")) md5all[15] += 42;
		if (!strcmp(arg, "-o")) { i++; continue; }
		if (!(l >= 2 && arg[l - 2] == '.'
		      && (arg[l - 1] == 'a' || arg[l - 1] == 'o')
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
			else if (!strchr(arg + 2, '-'))
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

#if GCC_RUN == 2
static void
create_hid_file (const char *linkid, const char *input, int outputfd)
{
	FILE *in = fopen(input, "r");
	if (!in)
		die("Could not open input file");

	FILE *out = fdopen(outputfd, "w");
	if (!out)
		die("Could not open output file");
	fprintf(out, ".section .note.GNU-stack,\"\",%%progbits\n");
	fprintf(out, ".section .__privplug_refs,\"e\",%%progbits\n");

	int nelim, nloc, nhid;
	char id[33];
	while (fscanf(in, "%d %d %d %32s", &nelim, &nloc, &nhid, id) == 4) {
		if (strncmp(id, linkid, 32)) {
			for (int i = 0; i < nelim + nloc + nhid; i++)
				fscanf(in, " %*[^\n]");
			continue;
		}

		for (int i = 0; i < nelim + nloc + nhid; i++) {
			const char *name;
			char tls;
			fscanf(in, " %*[^:]:%*[0-9a-f]:%*d:%c:%ms", &tls, &name);

			char *ver = strchr(name, '@');
			const char *pfx = ver ? "__privplug_" : "";
			if (ver) *ver = '_';
			fprintf(out, ".int %s%s-.\n", pfx, name);
			fprintf(out, ".hidden %s%s\n", pfx, name);
			if (tls == 'T')
				fprintf(out, ".type %s%s STT_TLS\n", pfx, name);
			if (ver) {
				fprintf(out, ".symver %s%s,", pfx, name);
				*ver = '@';
				fprintf(out, "%s\n", name);
			}
		}

		goto done;
	}

	// die("Could not find linkid %.32s\n", linkid);

done:
	fclose(in);
	fclose(out);
}

static char *
hid_file(const char *linkid)
{
	static char tmpl[] = "/tmp/\0_23456789abcdef0123456789abcdef-XXXXXX.o";
	memcpy(tmpl + strlen(tmpl), linkid, 32);
	int objfd = mkstemps(tmpl, 2);
	if (objfd < 0) return 0;
	unlink(tmpl);
	memcpy(strchr(tmpl, '-') + 1, "XXXXXX", 6);
	tmpl[sizeof tmpl - 2] = 's';
	int asmfd = mkstemps(tmpl, 2);
	if (asmfd < 0) return 0;
	static char objname[] = "/proc/self/fd/\0_23456789";
	snprintf(objname + strlen(objname), 11, "%u", 0u+objfd);

	create_hid_file(linkid, MERGED_PRIVDATA, asmfd);

	char *gcc_argv[] = {"gcc", "-c", tmpl, "-o", objname, 0};
	exec_child("/usr/bin/gcc", gcc_argv);
	unlink(tmpl);
	return objname;
}
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

	char *entry = "_start";
	char *outfile = "a.out";
	parse_entry_out(argc, argv, &entry, &outfile);

	char origcmd[7 + sizeof ALTDIR] = ALTDIR;
	strcpy(origcmd + sizeof ALTDIR - 1, name);

	char *storage[argc + 8];
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
	if (incremental_p && (GCC_RUN == 2 || GCC_RUN == 1)) {
		newargv = argv;
		goto exec;
	}

	newargv[0] = argv[0];
	/* GCC_RUN=0 does not use ld wrapper
	   GCC_RUN=1 normally copies argv into newargv except the exe name
	   GCC_RUN=2 reserves the slot for our obj file */
	memcpy(newargv + GCC_RUN, argv + 1, (argc - 1) * sizeof *argv);

#if GCC_RUN == 1
	int sockfd = daemon_connect(argc, argv, "Linker"[0]);
	snprintf(optstr + 32, sizeof optstr - 32, ":%s:%d", entry, sockfd);
	// Ignore for glibc
	if (access("/home/abuild/rpmbuild/SOURCES/linaro-glibc.spec", F_OK))
		newargv[argc++] = "--gc-sections";
	newargv[argc++] = "--plugin";
	newargv[argc++] = PLUGIN;
#else
	// TODO: The correct way is to place the aux file last on the linker's
	// command line, since undefined references in the aux file at the
	// beginning may affect extraction of object files from archives.
	// But it is not enough for ld.gold, since it erroneously picks up the
	// first visibility status it sees for a symbol and ignores the rest --
	// i.e. our .hidden directives.
	if (!(newargv[1] = hid_file(optstr)))
		die("failure creating aux input file");
	// Due to reserving first arg, actual argc is incremented
	argc++;
	newargv[argc++] = "--gc-sections";
	newargv[argc++] = "--plugin";
	newargv[argc++] = PLUGIN_PRIV;
	newargv[argc++] = newargv[1];
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
				      "-r", outfile, SRCID_FILE,
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
