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

typedef enum
{
  UNDEFINED,
  DYNAMIC,
  CONSTANT,
  PARTIALLY_CONSTANT
} resolve_lattice_t;

// TODO warning &dlsym &func->dlsym
struct signature
{
  const char* func_name;
  int sym_pos;
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
static hash_set<const char*, nofree_string_hash> considered_functions;
static call_symbols dynamic_symbols;

static resolve_lattice_t
parse_symbol (struct cgraph_node *node, gimple *stmt,
	      tree symbol, struct signature *sign);
static resolve_lattice_t
parse_gimple_stmt (struct cgraph_node *node, gimple* stmt, struct signature *sign);
void
dump_dynamic_symbol_calls (function* func, call_symbols *symbols);
void
dump_node (cgraph_node *node);
void
dump_lattice_value (FILE *outf, resolve_lattice_t val);

/* Compute the meet operator between VAL1 and VAL2:
   UNDEFINED M UNDEFINED          = UNDEFINED
   DYNAMIC   M UNDEFINED          = DYNAMIC
   DYNAMIC   M DYNAMIC            = DYNAMIC
   CONSTANT  M UNDEFINED          = CONSTANT
   CONSTANT  M CONSTANT           = CONSTANT
   DYNAMIC   M CONSTANT           = PARTIALLY_CONSTANT
   any       M PARTIALLY_CONSTANT = PARTIALLY_CONSTANT */
static resolve_lattice_t
resolve_lattice_meet (resolve_lattice_t val1, resolve_lattice_t val2)
{
  if (val1 == PARTIALLY_CONSTANT
      || val2 == PARTIALLY_CONSTANT
      || (val1 == DYNAMIC && val2 == CONSTANT)
      || (val1 == CONSTANT && val2 == DYNAMIC))
    return PARTIALLY_CONSTANT;

  if (val1 == val2)
    return val1;

  if (val1 == UNDEFINED)
    return val2;
  else if (val2 == UNDEFINED)
    return val1;

  gcc_unreachable ();
  return UNDEFINED;
}

static bool
is_considered_call (gimple *stmt)
{
  tree decl;
  tree t = gimple_call_fndecl (stmt);
  if (t && DECL_P (t))
    {
      decl = DECL_NAME (gimple_call_fndecl (stmt));
      if (considered_functions.contains (IDENTIFIER_POINTER (decl)))
	return true;
    }
  return false;
}

static bool
is_read_only (struct varpool_node *node)
{
  unsigned i;
  ipa_ref *ref = NULL;
  for (i = 0; node->iterate_referring (i, ref); i++)
    {
      /* Skip already considered functions */
      if (ref->use == IPA_REF_ADDR
	  && gimple_code (ref->stmt) == GIMPLE_CALL
	  && is_considered_call (ref->stmt))
	continue;

      if (ref->use != IPA_REF_LOAD)
	return false;
    }
  return true;
}

static bool
compare_ref (tree *t1, tree *t2)
{
  tree *p1 = t1, *p2 = t2;
  while (TREE_CODE (*p1) != VAR_DECL && TREE_CODE (*p2) != VAR_DECL)
    {
      if (TREE_CODE (*p1) != TREE_CODE (*p2)
	  || TREE_CODE (*p1) == VAR_DECL
	  || TREE_CODE (*p2) == VAR_DECL)
	return false;

      switch (TREE_CODE (*p1))
	{
	case ARRAY_REF:
	  /* Just skip it, we do not care about indeces */
	  break;

	case COMPONENT_REF:
	  if (TREE_OPERAND (*p1, 1) != TREE_OPERAND (*p2, 1))
	    return false;
	  break;

	default:
	  return false;
	}

      p1 = &TREE_OPERAND (*p1, 0);
      p2 = &TREE_OPERAND (*p2, 0);
    }
  return *p1 == *p2;
}

static void
unwind_expression_to_stack (tree *expr_p, auto_vec<tree, 10> *stack)
{
  tree *p;
  location_t loc = EXPR_LOCATION (*expr_p);
  for (p = expr_p; ; p = &TREE_OPERAND (*p, 0))
    {
      /* Fold INDIRECT_REFs now to turn them into ARRAY_REFs.  */
      if (TREE_CODE (*p) == INDIRECT_REF)
	*p = fold_indirect_ref_loc (loc, *p);

      if (TREE_CODE (*p) == ARRAY_REF || TREE_CODE (*p) == COMPONENT_REF)
	stack->safe_push (*p);
      else
	break;
    }
}

static resolve_lattice_t
collect_values (struct cgraph_node *node, tree *expr, struct signature *sign)
{
  resolve_lattice_t result = UNDEFINED;
  basic_block bb;
  gimple *stmt;

  FOR_EACH_BB_FN (bb, node->get_fun ())
    {
      gimple_stmt_iterator si;
      for (si = gsi_start_bb (bb); !gsi_end_p (si); gsi_next (&si))
	{
	  stmt = gsi_stmt (si);
	  /* If it is a single assignment, analyze rhs */
	  if (is_gimple_assign (stmt)
	      && (gimple_assign_single_p (stmt)
		  || gimple_assign_unary_nop_p (stmt)))
	    {
	      tree rhs = gimple_assign_rhs1 (stmt);
	      tree lhs = gimple_assign_lhs (stmt);

	      if (compare_ref (expr, &lhs) && !TREE_CLOBBER_P (rhs))
		{
		  resolve_lattice_t p_res = parse_symbol (node, stmt, rhs, sign);
		  result = resolve_lattice_meet (result, p_res);
		}
	    }
	}
    }

  return result;
}


static bool
contains_ref_expr (struct cgraph_node *node,  tree *expr)
{
  basic_block bb;
  unsigned j;
  gimple *stmt;
  tree base = get_base_address(*expr), *op;

  // TODO use dominators and stmt seq for more precise prediction.
  FOR_EACH_BB_FN (bb, node->get_fun ())
    {
      gimple_stmt_iterator si;
      for (si = gsi_start_bb (bb); !gsi_end_p (si); gsi_next (&si))
	{
	  stmt = gsi_stmt (si);
	  switch (gimple_code (stmt))
	    {
	    case GIMPLE_CALL:
	      /* Do not assume already checked calls */
	      if (is_considered_call (stmt))
		continue;

	      /* Check there is no address getting among arguments */
	      for (j = 0; j < gimple_call_num_args (stmt); ++j)
		{
		  tree arg = gimple_call_arg (stmt, j);
		  if (TREE_CODE (arg) == ADDR_EXPR
		      && base == get_base_address (TREE_OPERAND(arg, 0)))
		    return true;
		}
	      break;

	    case GIMPLE_ASSIGN:
	      /* Check there is no address getting among operands */
	      for (j = 0; j < gimple_num_ops (stmt); ++j)
		{
		  op = gimple_op_ptr (stmt, j);
		  if (TREE_CODE (*op) == ADDR_EXPR
		      && base == get_base_address (TREE_OPERAND(*op, 0)))
		    return true;
		  if (TREE_CODE (*op) == MEM_REF
		      && TREE_CODE (TREE_OPERAND (*op, 0)) == ADDR_EXPR
		      && TREE_OPERAND (TREE_OPERAND (*op, 0), 0) == base)
		    return true;
		}
	      break;

	    default:
	      continue;
	    }
	}
    }
  return false;
}

static resolve_lattice_t
parse_ref_1 (struct cgraph_node *node, gimple *stmt, struct signature *sign, 
	     tree ctor, auto_vec<tree, 10> *stack, unsigned HOST_WIDE_INT depth)
{
  unsigned HOST_WIDE_INT cnt;
  tree cfield, cval, field;
  tree t = (*stack)[depth];
  resolve_lattice_t result = UNDEFINED;
  /* At the bottom of the stack, string values should be */
  if (depth == 0)
    {
      switch (TREE_CODE (t))
	{
	case ARRAY_REF:
	  FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (ctor), cnt, cfield, cval)
	    {
	      resolve_lattice_t p_res = parse_symbol (node, stmt, cval, sign);
	      result = resolve_lattice_meet (result, p_res);
	    }
	  return result;
	  break;

	case COMPONENT_REF:
	  field = TREE_OPERAND (t, 1);
	  FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (ctor), cnt, cfield, cval)
	    if (field == cfield)
	      return parse_symbol (node, stmt, cval, sign);
	  gcc_unreachable ();
	  break;

	default:
	  return DYNAMIC;
	}
    }
  /* Dive into constructor */
  switch (TREE_CODE (t))
    {
    case ARRAY_REF:
      FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (ctor), cnt, cfield, cval)
	{
	  resolve_lattice_t p_res = parse_ref_1 (node, stmt, sign,
						 cval, stack, depth - 1);
	  result = resolve_lattice_meet (result, p_res);
	}
      break;

    case COMPONENT_REF:
      field = TREE_OPERAND (t, 1);
      FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (ctor), cnt, cfield, cval)
	if (field == cfield)
	  return parse_ref_1 (node, stmt, sign, cval, stack, depth - 1);
      gcc_unreachable ();
      break;

    default:
      result = resolve_lattice_meet (result, DYNAMIC);
    }
  return result;
}

static resolve_lattice_t
parse_ref (struct cgraph_node *node, gimple *stmt, 
	   tree *expr_p, struct signature *sign)
{
  tree base, ctor, t;
  auto_vec<tree, 10> expr_stack;

  unwind_expression_to_stack (expr_p, &expr_stack);
  t = expr_stack[expr_stack.length () - 1];
  base = TREE_OPERAND (t, 0);

  /* Do not parse if cannot reach base's decl */
  if (!DECL_P (base))
    return DYNAMIC;

  ctor = ctor_for_folding (base);
  /* Cannot find a constructor of the decl */
  if (ctor == NULL || ctor == error_mark_node)
    {
      /* Global var */
      if (TREE_STATIC (base) || DECL_EXTERNAL (base) || in_lto_p)
	{
	  if (is_read_only (varpool_node::get(base)) && DECL_INITIAL (base))
	    return parse_ref_1 (node, stmt, sign, DECL_INITIAL (base),
				&expr_stack, expr_stack.length () - 1);
	}

      if (contains_ref_expr (node, expr_p))
	return DYNAMIC;

      return collect_values (node, expr_p, sign);
    }
  return parse_ref_1 (node, stmt, sign, ctor, &expr_stack, expr_stack.length () - 1);
}

/* Parse function argument, make recursive step */
static resolve_lattice_t
parse_default_def (struct cgraph_node *node, tree default_def, struct signature *sign)
{
  resolve_lattice_t result = UNDEFINED;
  int arg_num;
  tree t, symbol, sym_decl = SSA_NAME_IDENTIFIER (default_def);
  struct cgraph_edge *cs;
  const char *caller_name, *subsymname = IDENTIFIER_POINTER (sym_decl);

  for (arg_num = 0, t = DECL_ARGUMENTS (node->get_fun ()->decl);
       t;
       t = DECL_CHAIN (t), arg_num++)
    if (DECL_NAME (t) == sym_decl)
      break;

  /* If no callers or DEFAULT_DEF is not represented in DECL_ARGUMENTS
     we cannot resolve the possible set of symbols */
  if (!node->callers || !t)
    return result;

  for (cs = node->callers; cs; cs = cs->next_caller)
    {
      resolve_lattice_t parse_result;
      struct signature subsign = {sign->func_name, arg_num};
      caller_name = cs->caller->asm_name ();

      /* FIXME recursive cycle is skipped until string are not handled,
	 otherwise it is incoorect */
      if (considered_functions.contains (caller_name))
	continue;

      if (dump_file)
	{
	  fprintf (dump_file, "\tTrack %s symbol obtained from:\n\t\t", subsymname);
	  print_gimple_stmt (dump_file, cs->call_stmt, 0, 0);
	}
      considered_functions.add (caller_name);
      symbol = gimple_call_arg (cs->call_stmt, arg_num);
      parse_result = parse_symbol (cs->caller, cs->call_stmt, symbol, &subsign);
      result = resolve_lattice_meet (result, parse_result);
      considered_functions.remove (caller_name);
    }
  return result;
}

static resolve_lattice_t
parse_symbol (struct cgraph_node *node, gimple *stmt, 
	      tree symbol, struct signature *sign)
{
  const char *symname;
  hash_set<const char*, nofree_string_hash> **symbols;
  gimple* def_stmt;
  enum tree_code code = TREE_CODE (symbol);

  switch (code)
    {
    case ADDR_EXPR:
      return parse_symbol (node, stmt, TREE_OPERAND (symbol, 0), sign);

    case STRING_CST:
      symname = TREE_STRING_POINTER (symbol);
      symbols = dynamic_symbols.get (sign->func_name);

      if (!symbols)
	{
	  auto empty_symbols = new hash_set<const char*, nofree_string_hash>;
	  dynamic_symbols.put (sign->func_name, empty_symbols);
	  symbols = &empty_symbols;
	}
      (*symbols)->add (symname);
      return CONSTANT;

    case SSA_NAME:
      if (SSA_NAME_IS_DEFAULT_DEF (symbol))
	return parse_default_def (node, symbol, sign);

      def_stmt = SSA_NAME_DEF_STMT (symbol);
      return parse_gimple_stmt (node, def_stmt, sign); 

    case ARRAY_REF:
    case COMPONENT_REF:
      return parse_ref (node, stmt, &symbol, sign);

    case VAR_DECL:
	{
	  /* Does a variable have a node in symtable  */
	  if (TREE_STATIC (symbol) || DECL_EXTERNAL (symbol) || in_lto_p)
	    {
	      varpool_node *sym_node = varpool_node::get (symbol);

	      if (sym_node->ctor_useable_for_folding_p ())
		return parse_symbol (node, stmt, ctor_for_folding (symbol), sign);

	      if (is_read_only (sym_node) && DECL_INITIAL (symbol))
		return parse_symbol (node, stmt, DECL_INITIAL (symbol), sign);

	      return DYNAMIC;
	    }
	  else if (!contains_ref_expr (node, &symbol))
	    {
	      return collect_values (node, &symbol, sign);
	    }
	  return DYNAMIC;
	}

    case INTEGER_CST:
      /* Allow null reference, permit another */
      // TODO null?
      return ! tree_to_shwi (symbol) ? CONSTANT : DYNAMIC;

    default:
      return DYNAMIC;
    }
}

static resolve_lattice_t
parse_gimple_stmt (struct cgraph_node *node, gimple* stmt, struct signature *sign)
{
  unsigned HOST_WIDE_INT i;
  resolve_lattice_t result = UNDEFINED;
  tree arg;

  switch (gimple_code (stmt))
    {
    case GIMPLE_ASSIGN:
      if (gimple_assign_single_p (stmt) 
	  || gimple_assign_unary_nop_p (stmt))
	{
	  arg = gimple_assign_rhs1 (stmt);
	  result = parse_symbol (node, stmt, arg, sign);
	}
      else
	result = resolve_lattice_meet (result, DYNAMIC);
      break;

    case GIMPLE_PHI:
      /*TODO phi cycles, currently unavalable because of absence of 
	string changes handling */
      if (dump_file)
	fprintf (dump_file, "\tPHI statement def: iterate each of them\n");
      for (i = 0; i < gimple_phi_num_args (stmt); i++)
	{
	  arg = gimple_phi_arg_def (stmt, i);
	  resolve_lattice_t p_res = parse_symbol (node, stmt, arg, sign);
	  result = resolve_lattice_meet (result, p_res);
	}
      break;

    case GIMPLE_CALL:
      arg = gimple_call_arg (stmt, sign->sym_pos);
      result = parse_symbol (node, stmt, arg, sign);
      break;

    default:
      result = resolve_lattice_meet (result, DYNAMIC);
      break;
    }
  return result;
}

static void 
process_calls (struct cgraph_node *node)
{
  unsigned HOST_WIDE_INT i;
  struct cgraph_edge *cs;

  if (dump_file)
    fprintf (dump_file, "Calls:\n");

  for (cs = node->callees; cs; cs = cs->next_callee)
    for (i = 0; i < signatures.length (); ++i)
      if (!strcmp (signatures[i].func_name, cs->callee->asm_name ()))
	{
	  resolve_lattice_t result;

	  if (dump_file)
	    fprintf (dump_file, "\t%s matched to the signature\n", 
		     cs->callee->asm_name ());

	  considered_functions.add (cs->callee->asm_name ());
	  result = parse_gimple_stmt (node, cs->call_stmt, &signatures[i]);
	  considered_functions.remove (cs->callee->asm_name ());

	  if (dump_file)
	    {
	      fprintf (dump_file, "\t%s set state:", cs->callee->asm_name ());
	      dump_lattice_value (dump_file, result);
	      fprintf (dump_file, "\n");
	    }
	}
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
    }

  virtual unsigned int execute(function *) { return resolve_dlsym_calls (); }
  }; // class pass_dlsym

} // anon namespace

void static
parse_argument (plugin_argument *arg)
{
  const char *arg_key = arg->key;
  const char *arg_value = arg->value;
  if (arg_key[0] == 's' && arg_key[1] == 'i'
      && arg_key[2] == 'g' && arg_key[3] == 'n'
      && arg_key[4] == '-')
    {
      struct signature initial = { &arg_key[5], atoi (arg_value) };
      signatures.safe_push (initial);
    }
}

void static
parse_arguments (int argc, plugin_argument *argv)
{
  int i;
  for (i = 0; i < argc; ++ i)
    parse_argument (&argv[i]);
}

int
plugin_init (plugin_name_args *plugin_info, plugin_gcc_version *version)
{
  if (!plugin_default_version_check (&gcc_version ,version))
    return 1; 

  parse_arguments (plugin_info->argc, plugin_info->argv);

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

void
dump_lattice_value (FILE *outf, resolve_lattice_t val)
{
  switch (val)
    {
    case UNDEFINED:
      fprintf (outf, "UNDEFINED");
      break;
    case DYNAMIC:
      fprintf (outf, "DYNAMIC");
      break;
    case CONSTANT:
      fprintf (outf, "CONSTANT");
      break;
    case PARTIALLY_CONSTANT:
      fprintf (outf, "PARTIALLY_CONSTANT");
      break;
    default:
      gcc_unreachable ();
    }
}
