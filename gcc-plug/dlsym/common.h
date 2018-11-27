// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#ifndef PLUG_H_INCLUDED
#define PLUG_H_INCLUDED

#include <cstdio>

#include "bversion.h"
#if BUILDING_GCC_VERSION < 4009
#error "This gcc is too old. The minimum required version is 4.9.x"
#endif

#if BUILDING_GCC_VERSION >= 6000
#include "gcc-plugin.h"
#else
#include "plugin.h"
#endif
#include "tree.h"
#include "basic-block.h"
#include "cgraph.h"
#include "gimple-pretty-print.h"
#include "tree-pass.h"
#include "internal-fn.h"
#include "gimple-expr.h"
#include "context.h"
#include "tree-ssa-alias.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "toplev.h"
#include "opts.h"
#include "md5.h"
#include "tree-cfg.h"
#include "output.h"
#include "diagnostic-core.h"

#if BUILDING_GCC_VERSION == 4009
static inline struct function *get_fun_cgraph_node (cgraph_node *node)
{
  struct function *fun = DECL_STRUCT_FUNCTION (node->decl);

  while (!fun && node->clone_of)
    {
      node = node->clone_of;
      fun = DECL_STRUCT_FUNCTION (node->decl);
    }

  return fun;
}
#endif

#if BUILDING_GCC_VERSION >= 5000
#define varpool_get_node(decl) varpool_node::get(decl)
#if BUILDING_GCC_VERSION >= 8001
#define dump_symtab(f) symtab->dump (f)
#else
#define dump_symtab(f) symtab_node::dump_table (f)
#endif
#define dump_cgraph_node(file, node) (node)->dump(file)
#define get_fun_cgraph_node(node) (node)->get_fun ()
#define cgraph_function_with_gimple_body_p(node) node->has_gimple_body_p ()
#define fatal_error(...) fatal_error (UNKNOWN_LOCATION, __VA_ARGS__)

#if BUILDING_GCC_VERSION >= 6000
typedef gimple *gimple_ptr;
typedef const gimple *const_gimple_ptr;
#define gimple gimple_ptr
#define const_gimple const_gimple_ptr
#endif

#if BUILDING_GCC_VERSION < 8001
#define TDF_NONE 0
#endif

/* IPA/LTO related */
#define ipa_ref_list_referring_iterate(L, I, P) \
	(L)->referring.iterate((I), &(P))
#define ipa_ref_list_reference_iterate(L,I,P) \
	vec_safe_iterate ((L)->references, (I), &(P))
#endif

static inline const char *
assemble_name_raw (struct symtab_node *node)
{
  const char *name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (node->decl));
  return name + (name[0] == '*');
}

#endif
