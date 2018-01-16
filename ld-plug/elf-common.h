#ifndef ELF_COMMON_H
#define ELF_COMMON_H

#include "confdef.h"

/* Some versions of the elf headers define it as signed int.  */
#undef SHF_EXCLUDE
#define SHF_EXCLUDE	     (1U << 31)

#ifndef PLUG_TARGET_ELFCLASS
#if UINTPTR_MAX == UINT64_MAX
#define PLUG_TARGET_ELFCLASS 64
#elif UINTPTR_MAX == UINT32_MAX
#define PLUG_TARGET_ELFCLASS 32
#endif
#endif

#if PLUG_TARGET_ELFCLASS == 64
#define ElfNN_(t) Elf64_##t
#define ELFCLASS ELFCLASS64
#define ELF_ST_BIND(st_info)        ELF64_ST_BIND(st_info)
#define ELF_ST_TYPE(st_info)        ELF64_ST_TYPE(st_info)
#define ELF_ST_VISIBILITY(st_other) ELF64_ST_VISIBILITY(st_other)
#define ELF_R_SYM(r_info)           ELF64_R_SYM(r_info)
#elif PLUG_TARGET_ELFCLASS == 32
#define ElfNN_(t) Elf32_##t
#define ELFCLASS ELFCLASS32
#define ELF_ST_BIND(st_info)        ELF32_ST_BIND(st_info)
#define ELF_ST_TYPE(st_info)        ELF32_ST_TYPE(st_info)
#define ELF_ST_VISIBILITY(st_other) ELF32_ST_VISIBILITY(st_other)
#define ELF_R_SYM(r_info)           ELF32_R_SYM(r_info)
#endif

#define ELFDATA ELFDATA2LSB

#define PLUG_SECTION_PREFIX ".comment.privplugid."

static const char *
plug_srcid(const char *name)
{
	if (strncmp (name, PLUG_SECTION_PREFIX, strlen (PLUG_SECTION_PREFIX)))
		return 0;
	return name + strlen (PLUG_SECTION_PREFIX);
}
#endif
