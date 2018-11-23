// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#define PLUG_SECTION_PREFIX ".comment.privplugid."

/* TODO: hoist PLUG_SECTION_PREFIX definition, #include "common.h" */

asm("\t.pushsection\t" PLUG_SECTION_PREFIX
    "0123456789abcdef0123456789abcdef,\"e\",%note\n"
    "\t.popsection\n");
