#include "common.h"
#include "symbols-pass.h"

vec<struct signature> signatures;
vec<struct resolve_ctx *> resolve_contexts;

static resolve_lattice_t parse_symbol (struct resolve_ctx *ctx, gimple stmt, tree symbol);
static resolve_lattice_t parse_gimple_stmt (struct resolve_ctx *ctx, gimple stmt);

static void
init_resolve_ctx (struct resolve_ctx *ctx)
{
  ctx->status = UNDEFINED;
  ctx->considered_functions = new vec<const char *> ();
  ctx->symbols = new vec<const char *> ();
  ctx->calls_chain = new vec<call_info> ();
}

static void
free_resolve_ctx (struct resolve_ctx *ctx)
{
  free (ctx->considered_functions);
  free (ctx->symbols);
  free (ctx->calls_chain);
}

static struct call_info *
get_current_call_info (struct resolve_ctx *ctx)
{
  if (ctx->calls_chain->length ())
    return &ctx->calls_chain->last ();
  return NULL;
}

static void
push_call_info (struct resolve_ctx *ctx, struct cgraph_node *node,
		gimple stmt, struct signature *sign)
{
  struct call_info call = { node, stmt, sign };
  ctx->calls_chain->safe_push (call);
  ctx->considered_functions->safe_push (sign->func_name);
}

static void
pop_call_info (struct resolve_ctx *ctx)
{
  ctx->calls_chain->pop ();
  ctx->considered_functions->pop ();
}

static bool
vec_constains_str (vec<const char *> *v, const char *str)
{
  unsigned i;
  const char *vstr;

  for (i = 0; v->iterate (i, &vstr); ++i)
    if (!strcmp (vstr, str))
      return true;

  return false;
}

static bool
vec_add_unique_str (vec<const char *> *v, const char *str)
{
  unsigned i;
  const char *vstr;

  for (i = 0; v->iterate (i, &vstr); ++i)
    if (!strcmp (vstr, str))
      return false;

  v->safe_push (str);
  return true;
}

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
is_considered_call (struct resolve_ctx *ctx, gimple stmt)
{
  tree decl;
  tree t = gimple_call_fndecl (stmt);
  if (t && DECL_P (t))
    {
      decl = DECL_ASSEMBLER_NAME (gimple_call_fndecl (stmt));
      if (vec_constains_str (ctx->considered_functions, IDENTIFIER_POINTER (decl)))
	return true;
    }
  return false;
}

static bool
is_read_only (struct resolve_ctx *ctx, struct varpool_node *node)
{
  unsigned i;
  ipa_ref *ref = NULL;
  for (i = 0; ipa_ref_list_referring_iterate (&node->ref_list, i, ref); i++)
    {
      /* Skip already considered functions */
      if (ref->use == IPA_REF_ADDR
	  && gimple_code (ref->stmt) == GIMPLE_CALL
	  && is_considered_call (ctx, ref->stmt))
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

/* Collect values from assignment statements for global variable */
static resolve_lattice_t
collect_values_global (struct resolve_ctx *ctx, struct varpool_node *node)
{
  unsigned i;
  ipa_ref *ref = NULL;
  resolve_lattice_t result = UNDEFINED;
  for (i = 0; ipa_ref_list_referring_iterate (&node->ref_list, i, ref); i++)
    if (ref->use == IPA_REF_STORE)
      result = resolve_lattice_meet (result, parse_gimple_stmt (ctx, ref->stmt));
  return result;
}

/* Collect values from assignment statements in GET_CURRENT_CALL_INFO (CTX)
   for local variable */
static resolve_lattice_t
collect_values_local (struct resolve_ctx *ctx, tree *expr_p)
{
  resolve_lattice_t result = UNDEFINED;
  basic_block bb;
  gimple stmt;
  struct call_info *call = get_current_call_info (ctx);

  FOR_EACH_BB_FN (bb, get_fun_cgraph_node (call->node))
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

	      if (compare_ref (expr_p, &lhs) && !TREE_CLOBBER_P (rhs))
		{
		  resolve_lattice_t p_res = parse_symbol (ctx, stmt, rhs);
		  result = resolve_lattice_meet (result, p_res);
		}
	    }
	}
    }

  return result;
}


static bool
contains_ref_expr (struct resolve_ctx *ctx,  tree *expr_p)
{
  basic_block bb;
  unsigned j;
  gimple stmt;
  tree base = get_base_address (*expr_p), *op;
  struct cgraph_node *node = get_current_call_info (ctx)->node;

  // TODO use dominators and stmt seq for more precise prediction.
  FOR_EACH_BB_FN (bb, get_fun_cgraph_node (node))
    {
      gimple_stmt_iterator si;
      for (si = gsi_start_bb (bb); !gsi_end_p (si); gsi_next (&si))
	{
	  stmt = gsi_stmt (si);
	  switch (gimple_code (stmt))
	    {
	    case GIMPLE_CALL:
	      /* Do not assume already checked calls */
	      if (is_considered_call (ctx, stmt))
		continue;

	      /* Check there is no address getting among arguments */
	      for (j = 0; j < gimple_call_num_args (stmt); ++j)
		{
		  tree arg = gimple_call_arg (stmt, j);
		  if (TREE_CODE (arg) == ADDR_EXPR
		      && base == get_base_address (TREE_OPERAND (arg, 0)))
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
parse_ref_1 (struct resolve_ctx *ctx, gimple stmt, tree ctor,
	     auto_vec<tree, 10> *stack, unsigned HOST_WIDE_INT depth)
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
	      resolve_lattice_t p_res = parse_symbol (ctx, stmt, cval);
	      result = resolve_lattice_meet (result, p_res);
	    }
	  return result;
	  break;

	case COMPONENT_REF:
	  field = TREE_OPERAND (t, 1);
	  FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (ctor), cnt, cfield, cval)
	    if (field == cfield)
	      return parse_symbol (ctx, stmt, cval);
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
	  resolve_lattice_t p_res = parse_ref_1 (ctx, stmt, cval,
						 stack, depth - 1);
	  result = resolve_lattice_meet (result, p_res);
	}
      break;

    case COMPONENT_REF:
      field = TREE_OPERAND (t, 1);
      FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (ctor), cnt, cfield, cval)
	if (field == cfield)
	  return parse_ref_1 (ctx, stmt, cval, stack, depth - 1);
      gcc_unreachable ();
      break;

    default:
      result = resolve_lattice_meet (result, DYNAMIC);
    }
  return result;
}

static resolve_lattice_t
parse_ref (struct resolve_ctx *ctx, gimple stmt, tree *expr_p)
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
      resolve_lattice_t result = UNDEFINED;
      /* Global var */
      if (TREE_STATIC (base) || DECL_EXTERNAL (base) || in_lto_p)
	{
	  if (!is_read_only (ctx, varpool_get_node (base)))
	    result = resolve_lattice_meet (result, DYNAMIC);

	  tree init = DECL_INITIAL (base);
	  if (init)
	    {
	      resolve_lattice_t parse_r;
	      parse_r = parse_ref_1 (ctx, stmt, DECL_INITIAL (base),
				     &expr_stack, expr_stack.length () - 1);
	      result = resolve_lattice_meet (result, parse_r);
	    }
	  /* NOTE: do not collect values for COMPONENT_REF */
	  return result;
	}

      if (contains_ref_expr (ctx, expr_p))
	result = resolve_lattice_meet (result, DYNAMIC);

      return resolve_lattice_meet (result, collect_values_local (ctx, expr_p));
    }
  return parse_ref_1 (ctx, stmt, ctor, &expr_stack, expr_stack.length () - 1);
}

/* Parse function argument, make recursive step */
static resolve_lattice_t
parse_default_def (struct resolve_ctx *ctx, tree default_def)
{
  resolve_lattice_t result = UNDEFINED;
  int arg_num;
  struct cgraph_edge *cs;
  struct call_info *call = get_current_call_info (ctx);
  tree t, symbol, sym_decl = SSA_NAME_IDENTIFIER (default_def);
  const char *caller_name, *subsymname = IDENTIFIER_POINTER (sym_decl);

  for (arg_num = 0, t = DECL_ARGUMENTS (get_fun_cgraph_node (call->node)->decl);
       t;
       t = DECL_CHAIN (t), arg_num++)
    if (DECL_NAME (t) == sym_decl)
      break;

  /* If no callers or DEFAULT_DEF is not represented in DECL_ARGUMENTS
     we cannot resolve the possible set of symbols */
  if (!call->node->callers || !t)
    return result;

  for (cs = call->node->callers; cs; cs = cs->next_caller)
    {
      resolve_lattice_t parse_result;
      caller_name = assemble_name_raw (call->node);
      struct signature subsign = {caller_name, arg_num};

      /* FIXME recursive cycle is skipped until string are not handled,
	 otherwise it is incoorect */
      if (vec_constains_str (ctx->considered_functions, caller_name))
	continue;

      if (dump_file)
	{
	  fprintf (dump_file, "\tTrack %s symbol obtained from:\n\t\t", subsymname);
	  print_gimple_stmt (dump_file, cs->call_stmt, 0, 0);
	}
      push_call_info (ctx, cs->caller, cs->call_stmt, &subsign);

      symbol = gimple_call_arg (cs->call_stmt, arg_num);
      parse_result = parse_symbol (ctx, cs->call_stmt, symbol);
      result = resolve_lattice_meet (result, parse_result);

      pop_call_info (ctx);
    }
  return result;
}

static resolve_lattice_t
parse_symbol (struct resolve_ctx *ctx, gimple stmt, tree symbol)
{
  gimple def_stmt;
  enum tree_code code = TREE_CODE (symbol);

  switch (code)
    {
    case ADDR_EXPR:
      return parse_symbol (ctx, stmt, TREE_OPERAND (symbol, 0));

    case STRING_CST:
      vec_add_unique_str (ctx->symbols, TREE_STRING_POINTER (symbol));
      return CONSTANT;

    case SSA_NAME:
      if (SSA_NAME_IS_DEFAULT_DEF (symbol))
	return parse_default_def (ctx, symbol);

      def_stmt = SSA_NAME_DEF_STMT (symbol);
      return parse_gimple_stmt (ctx, def_stmt);

    case ARRAY_REF:
    case COMPONENT_REF:
      return parse_ref (ctx, stmt, &symbol);

    case VAR_DECL:
	{
	  resolve_lattice_t result = UNDEFINED;
	  /* Does a variable have a node in symtable  */
	  if (TREE_STATIC (symbol) || DECL_EXTERNAL (symbol) || in_lto_p)
	    {
	      tree init;
	      varpool_node *sym_node = varpool_get_node (symbol);

	      if ((init = ctor_for_folding (symbol)) != error_mark_node)
		return parse_symbol (ctx, stmt, init);

	      if (!is_read_only (ctx, sym_node))
		result = resolve_lattice_meet (result, DYNAMIC);

	      init = DECL_INITIAL (symbol);
	      if (init)
		{
		  resolve_lattice_t parse_r = parse_symbol (ctx, stmt, init);
		  result = resolve_lattice_meet (result, parse_r);
		}

	      return resolve_lattice_meet (result,
					   collect_values_global (ctx, sym_node));
	    }

	  if (contains_ref_expr (ctx, &symbol))
	    result = resolve_lattice_meet (result, DYNAMIC);

	  return resolve_lattice_meet (result, collect_values_local (ctx, &symbol));
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
parse_gimple_stmt (struct resolve_ctx *ctx, gimple stmt)
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
	  result = parse_symbol (ctx, stmt, arg);
	}
      else
	result = resolve_lattice_meet (result, DYNAMIC);
      break;

    case GIMPLE_PHI:
      /*TODO phi cycles are currently unavalable because of absence of
	string changes handling */
      if (dump_file)
	fprintf (dump_file, "\tPHI statement def: iterate each of them\n");
      for (i = 0; i < gimple_phi_num_args (stmt); i++)
	{
	  arg = gimple_phi_arg_def (stmt, i);
	  resolve_lattice_t p_res = parse_symbol (ctx, stmt, arg);
	  result = resolve_lattice_meet (result, p_res);
	}
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
  tree symbol;

  if (dump_file)
    fprintf (dump_file, "Calls:\n");

  for (cs = node->callees; cs; cs = cs->next_callee)
    for (i = 0; i < signatures.length (); ++i)
      if (!strcmp (signatures[i].func_name, assemble_name_raw (cs->callee)))
	{
	  if (dump_file)
	    fprintf (dump_file, "\t%s matched to the signature\n",
		     assemble_name_raw (cs->callee));

	  resolve_ctx *ctx = XNEW (struct resolve_ctx);
	  init_resolve_ctx (ctx);
	  ctx->base_sign = &signatures[i];
	  ctx->loc = gimple_location (cs->call_stmt);
	  ctx->node = node;

	  push_call_info (ctx, node, cs->call_stmt, &signatures[i]);

	  symbol = gimple_call_arg (cs->call_stmt, signatures[i].sym_pos);
	  ctx->status = parse_symbol (ctx, cs->call_stmt, symbol);

	  pop_call_info (ctx);

	  if (dump_file)
	    {
	      fprintf (dump_file, "\t%s set state:", assemble_name_raw (cs->callee));
	      dump_lattice_value (dump_file, ctx->status);
	      fprintf (dump_file, "\n");
	      dump_dynamic_symbol_calls (ctx);
	    }

	  resolve_contexts.safe_push (ctx);
	}
}

static unsigned int
resolve_dlsym_calls (void)
{
  struct cgraph_node *node;

  // Fix the bodies and call graph
  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node)
    // TODO handle inlined functions during recurive traversing
    if (!node->global.inlined_to)
      cgraph_get_body (node);

  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node)
    {
      if (dump_file)
	dump_node (node);

      process_calls (node);
    }

  if (dump_file)
    fprintf (dump_file, "Symbols pass ended\n\n");
  return 0;
}

/* Resolve dlsyms calls */

namespace
{
  const pass_data pass_data_symbols =
    {
      SIMPLE_IPA_PASS,
      "symbols",		/* name */
      OPTGROUP_NONE,		/* optinfo_flags */
#if BUILDING_GCC_VERSION == 4009
      false,			/* has_gate */
      true,			/* has_execute */
#endif
      TV_NONE,			/* tv_id */
      0,                        /* properties_required */
      0,			/* properties_provided */
      0,			/* properties_destroyed */
      0,                        /* todo_flags_start */
      0				/* todo_flags_finish */
    };

  class pass_symbols : public simple_ipa_opt_pass
  {
public:
  pass_symbols (gcc::context *ctxt)
    : simple_ipa_opt_pass (pass_data_symbols, ctxt)
    {
    }

#if BUILDING_GCC_VERSION >= 5000
  virtual unsigned int execute(function *) { return resolve_dlsym_calls (); }
#else
  virtual unsigned int execute(void) { return resolve_dlsym_calls (); }
#endif
  }; // class pass_dlsym

} // anon namespace

simple_ipa_opt_pass *
make_pass_symbols (gcc::context *ctxt)
{
  return new pass_symbols (ctxt);
}

void
finalize_pass_symbols ()
{
  while (!resolve_contexts.is_empty ())
    free_resolve_ctx (resolve_contexts.pop ());

  while (!signatures.is_empty ())
    free ((char *) signatures.pop ().func_name);
}

void
dump_dynamic_symbol_calls (struct resolve_ctx *ctx)
{
  unsigned i;
  const char *symbol;
  if (ctx->symbols->is_empty ())
    {
      fprintf(dump_file, "\n\n");
      return;
    }

  fprintf (dump_file, "File:Line:Function->Callee->[Symbols]:\n");
  fprintf (dump_file, "\t%s:%d:%s->%s->[",
           LOCATION_FILE (ctx->loc), LOCATION_LINE (ctx->loc),
	   assemble_name_raw (ctx->node), ctx->base_sign->func_name);
  for (i = 0; ctx->symbols->iterate (i, &symbol); ++i)
    {
      if (i)
	fprintf (dump_file, ",");
      fprintf (dump_file, "%s", symbol);
    }
  fprintf(dump_file, "]\n\n");
}

void
dump_node (struct cgraph_node *node)
{
  fprintf (dump_file, "Call graph of a node:\n");
  dump_cgraph_node (dump_file, node);
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

void
dump_decl_section (FILE *outf, tree decl)
{
  const char *section_name;
  section *sect = function_section (decl);
  gcc_assert (sect != NULL);

  if (sect->common.flags & SECTION_NAMED)
    section_name = sect->named.name;
  else if (sect == text_section)
    section_name = ".text";
  else
    gcc_unreachable ();

  fprintf (outf, "%s", section_name);
}

