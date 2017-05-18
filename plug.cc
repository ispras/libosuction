#include <iostream>
#include <gcc-plugin.h>
#include <plugin-version.h>

#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "tree.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "is-a.h"
#include "predict.h"
#include "basic-block.h"
#include "gimple-expr.h"
#include "gimple.h"
#include "gimple-pretty-print.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "hash-map.h"
#include "hash-set.h"
#include "cgraph.h"
int plugin_is_GPL_compatible;

// TODO warning &dlsym &func->dlsym
// TODO overriden functions (same names, different sym_pos)
struct signature
{
  const char* func_name;
  size_t sym_pos;
};

/* TODO add considered_functions and change the parse_call signature
   according to the call info */
struct call_info
{
  /* Function, where dynamic call appears  */
  function* func;
  /* Signature of dynamic call */
  struct signature sign;
};

typedef hash_map<const char*, hash_set<const char*, nofree_string_hash>*> call_symbols;

static vec<struct signature> signatures;
static hash_set<const char*> considered_functions;
static call_symbols dynamic_symbols;

void dump_dynamic_symbol_calls (function* func, call_symbols *symbols);
void dump_node (cgraph_node *node);

static bool 
parse_symbol (struct cgraph_node *node, tree symbol, struct signature *sign)
{
  /* Constant string */
  if (TREE_CODE (symbol) == ADDR_EXPR &&
      TREE_CODE (TREE_OPERAND (symbol, 0)) == STRING_CST)
    {
      const char *symname = TREE_STRING_POINTER (TREE_OPERAND (symbol, 0));
      auto symbols = dynamic_symbols.get (sign->func_name);

      if (!symbols)
	{
	  auto empty_symbols = new hash_set<const char*, nofree_string_hash>;
	  dynamic_symbols.put (sign->func_name, empty_symbols);
	  symbols = &empty_symbols;
	}
      (*symbols)->add (symname);
      return true;
    }		   

  if (TREE_CODE (symbol) == SSA_NAME)
    {
      bool result = true;
      unsigned HOST_WIDE_INT i;
      gimple* def_stmt;

      /* Parse function argument, make recursive step */
      if (SSA_NAME_IS_DEFAULT_DEF (symbol))
	{
	  unsigned HOST_WIDE_INT arg_num;
	  tree t;
	  tree sym_decl = SSA_NAME_IDENTIFIER (symbol);
	  struct cgraph_edge *cs;
	  const char *caller_name, *subsymname = IDENTIFIER_POINTER (sym_decl);

	  for (arg_num = 0, t = DECL_ARGUMENTS (node->get_fun ()->decl);
	       t;
	       t = DECL_CHAIN (t), arg_num++)
	    if (DECL_NAME (t) == sym_decl)
	      break;

	  /* If no callers or SYMBOL are not represented in DECL_ARGUMENTS
	     we cannot resolve the possible set of symbols */
	  if (!node->callers || !t)
	    return false;

	  for (cs = node->callers; cs; cs = cs->next_caller)
	    {
	      struct signature subsign = {sign->func_name, arg_num};
	      caller_name = IDENTIFIER_POINTER (DECL_NAME (cs->caller->get_fun ()->decl));

	      /* FIXME recursive cycle is skipped until string are not handled,
		 otherwise it is incoorect */
	      if (considered_functions.contains (caller_name))
		continue;

	      if (dump_file)
		{
		  fprintf (dump_file, "\tTrack %s symbol obtained from:\n\t\t",
			   subsymname);
		  print_gimple_stmt (dump_file, cs->call_stmt, 0, 0);
		}
	      // TODO indirect calls
	      considered_functions.add (caller_name);
	      tree symbol1 = gimple_call_arg (cs->call_stmt, arg_num);
	      result &= parse_symbol (cs->caller, symbol1, &subsign);
	      considered_functions.remove (caller_name);
	    }
	  return result;
	}

      /* Go through definitions chain and try to determine the set of possible values */
      def_stmt = SSA_NAME_DEF_STMT (symbol);
      switch (gimple_code (def_stmt))
	{
	case GIMPLE_ASSIGN:
	  return false;

	case GIMPLE_PHI:
	  /*TODO phi cycles, currently anavalable because of absence of 
	    string changing track */
	  if (dump_file)
	    fprintf (dump_file, "\tPHI statement def: iterate each of them\n");
	  for (i = 0; i < gimple_phi_num_args (def_stmt); i++)
	    {
	      tree arg = gimple_phi_arg_def (def_stmt, i);
	      result &= parse_symbol (node, arg, sign);
	    }
	  return result;

	default:
	  return false;
	}
      // TODO handle simple expressions (global_const.c)
      return false;
    }
  return false;
}

static void 
process_calls (struct cgraph_node *node)
{
  unsigned HOST_WIDE_INT i;
  struct cgraph_edge *cs;
  bool is_limited;

  if (dump_file)
    fprintf (dump_file, "Calls:\n");

  for (cs = node->callees; cs; cs = cs->next_callee)
    for (i = 0; i < signatures.length (); ++i)
      if (!strcmp (signatures[i].func_name, cs->callee->asm_name ()))
	{
	  if (dump_file)
	    fprintf (dump_file, "\t%s matched to the signature\n", 
		     cs->callee->asm_name ());

	  tree symbol = gimple_call_arg (cs->call_stmt, signatures[i].sym_pos);
	  is_limited = parse_symbol (node, symbol, &signatures[i]); 

	  if (dump_file && !is_limited) 
	    fprintf (dump_file, "\t%s set is not limited\n", 
		     cs->callee->asm_name ());
	}
  if (dump_file)
    fprintf (dump_file, "\n");
}

static unsigned int 
resolve_dlsym_calls (void)
{
  struct cgraph_node* node;

  // Fix the bodies and call graph
  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node)
    // TODO handle inlined functions during recurive traversing
    if (!node->global.inlined_to)
      node->get_body ();

  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node)
    {
      if (dump_file)
	dump_node (node);

      process_calls (node);

      if (dump_file && dynamic_symbols.elements ())
	dump_dynamic_symbol_calls (node->get_fun (), &dynamic_symbols);

      // Free stored data
      for (call_symbols::iterator it = dynamic_symbols.begin (); 
	   it != dynamic_symbols.end ();
	   ++it)
	delete (*it).second;
      dynamic_symbols.empty ();
    }

  if (dump_file)
    fprintf (dump_file, "Dynamic symbol resolving pass ended\n\n");
  return 0;
}

/* Resolve dlsyms calls */

namespace
{
  const pass_data pass_dlsym_data = 
    {
      SIMPLE_IPA_PASS,
      "dlsym",			/* name */
      OPTGROUP_NONE,		/* optinfo_flags */
      TV_NONE,			/* tv_id */
      0,                        /* properties_required */
      0,			/* properties_provided */
      0,			/* properties_destroyed */
      0,                        /* todo_flags_start */
      0				/* todo_flags_finish */
    };

  class pass_dlsym : public simple_ipa_opt_pass
  {
public:
  pass_dlsym(gcc::context *ctxt)
    : simple_ipa_opt_pass (pass_dlsym_data, ctxt)
    {
      struct signature initial = { .func_name = "dlsym",  .sym_pos = 1 };
      signatures.safe_push (initial);
    }

  virtual unsigned int execute(function *) { return resolve_dlsym_calls (); }
  }; // class pass_dlsym

} // anon namespace

int
plugin_init (plugin_name_args *plugin_info, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (&gcc_version ,version))
    return 1; 

  struct register_pass_info pass_info;
  pass_info.pass = new pass_dlsym (g);
  pass_info.reference_pass_name = "materialize-all-clones";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback (plugin_info->base_name,
		     PLUGIN_PASS_MANAGER_SETUP, NULL,
		     &pass_info);
  return 0;
}

void
dump_dynamic_symbol_calls (function* func, call_symbols *symbols)
{
  if (!symbols->elements ())
    {
      fprintf(dump_file, "\n\n");
      return;
    }

  fprintf (dump_file, "Function->callee->[symbols]:\n");
  for (auto it = symbols->begin (); 
       it != symbols->end ();
       ++it)
    {
      fprintf(dump_file, "\t%s->%s->[", function_name(func),
	      (*it).first);
      for (auto it2 = (*it).second->begin (); 
	   it2 != (*it).second->end ();
	   ++it2)
	{
	  if (it2 != (*it).second->begin ())
	    fprintf (dump_file, ",");
	  fprintf (dump_file, "%s", *it2); 
	}
      fprintf(dump_file, "]\n");
    }
  fprintf(dump_file, "\n\n");
}

void 
dump_node (cgraph_node *node)
{
  fprintf (dump_file, "Call graph of a node:\n");
  node->dump (dump_file);
  fprintf (dump_file, "\n");
}

