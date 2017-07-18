#ifndef PLUG_H_INCLUDED
#define PLUG_H_INCLUDED

#include "bversion.h"
#if BUILDING_GCC_VERSION < 4009
#error "This gcc is too old. The minimum required version is 4.9.x"
#endif

#if BUILDING_GCC_VERSION >= 6000
#include "gcc-plugin.h"
#else
#include "plugin.h"
#endif
#include "plugin-version.h"
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
#define cgraph_get_body(node) (node)->get_body()
#define varpool_get_node(decl) varpool_node::get(decl)
#define dump_symtab(f) symtab_node::dump_table (f)
#define dump_cgraph_node(file, node) (node)->dump(file)
#define get_fun_cgraph_node(node) (node)->get_fun ()
#define cgraph_function_with_gimple_body_p(node) node->has_gimple_body_p ()

#if BUILDING_GCC_VERSION >= 6000
typedef gimple *gimple_ptr;
typedef const gimple *const_gimple_ptr;
#define gimple gimple_ptr
#define const_gimple const_gimple_ptr
#endif

/* IPA/LTO related */
#define ipa_ref_list_referring_iterate(L, I, P)	\
	(L)->referring.iterate((I), &(P))
#endif

#endif
