#define PLUG_SECTION_PREFIX ".comment.privplugid."

/* TODO: hoist PLUG_SECTION_PREFIX definition, #include "common.h" */

asm("\t.pushsection\t" PLUG_SECTION_PREFIX
    "0123456789abcdef0123456789abcdef,\"e\",%note\n"
    "\t.popsection\n");
