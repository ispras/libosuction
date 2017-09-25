#include "common.h"
#include "jfunc-pass.h"

vec<struct jfunction *> jfuncs;

static void
compute_jf_for_edge (struct cgraph_edge *cs)
{
  struct jfunction *jf;
  gimple call = cs->call_stmt;
  unsigned n, k, arg_num = gimple_call_num_args (call);

  if (arg_num == 0)
    return;

  if (gimple_call_internal_p (call))
    return;

  for (n = 0; n < arg_num; ++n)
    {
      tree arg = gimple_call_arg (call, n);
      if (TREE_CODE (arg) == SSA_NAME && SSA_NAME_IS_DEFAULT_DEF (arg))
	{
	  tree t = DECL_ARGUMENTS (get_fun_cgraph_node (cs->caller)->decl);
	  for (k = 0; t; t = DECL_CHAIN (t), k++)
	    if (DECL_NAME (t) == SSA_NAME_IDENTIFIER (arg))
	      break;

	  if (!t)
	    continue;

	  jf = XNEW (struct jfunction);
	  jf->from_name = assemble_name_raw (cs->caller);
	  jf->from_arg = k;
	  jf->to_name = assemble_name_raw (cs->callee);
	  jf->to_arg = n;

	  jfuncs.safe_push (jf);

	  if (dump_file)
	    fprintf (dump_file, "%s,%d->%s,%d\n",
		     jf->from_name, jf->from_arg, jf->to_name, jf->to_arg);
	}
    }
}

static unsigned int
collect_jfunc (void)
{
  struct cgraph_node *node;

  /* Transforms were obligatory applied after each pass
     until version 5000. Here we have to apply them manually
     due to benefits from previous ipa passes. */
#if BUILDING_GCC_VERSION >= 5000
  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node)
    if (!node->global.inlined_to)
      node->get_body();
#endif

  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (node)
    {
      struct cgraph_edge *cs;

      if (dump_file)
	{
	  fprintf (dump_file, "\nCall graph of a node:\n");
	  dump_cgraph_node (dump_file, node);
	  fprintf (dump_file, "\n");
	}

      for (cs = node->callees; cs; cs = cs->next_callee)
	compute_jf_for_edge (cs);

      /* It is the place to handle indirect calls */
    }

  if (dump_file)
    fprintf (dump_file, "\n\nJump Function pass ended\n\n");
  return 0;
}

namespace
{
  const pass_data pass_jfunc_data =
    {
      SIMPLE_IPA_PASS,
      "jfunc",			/* name */
      OPTGROUP_NONE,		/* optinfo_flags */
#if BUILDING_GCC_VERSION == 4009
      false,			/* has_gate */
      true,			/* has_execute */
#endif
      TV_NONE,			/* tv_id */
      0,			/* properties_required */
      0,			/* properties_provided */
      0,			/* properties_destroyed */
      0,			/* todo_flags_start */
      0 			/* todo_flags_finish */
    };

  class pass_jfunc : public simple_ipa_opt_pass
  {
public:
  pass_jfunc (gcc::context *ctxt)
    : simple_ipa_opt_pass (pass_jfunc_data, ctxt)
    {
    }

#if BUILDING_GCC_VERSION >= 5000
  virtual unsigned int execute (function *) { return collect_jfunc (); }
#else
  virtual unsigned int execute (void) { return collect_jfunc (); }
#endif
  }; // class pass_dsigns

} // anon namespace

simple_ipa_opt_pass *
make_pass_jfunc (gcc::context *ctxt)
{
  return new pass_jfunc (ctxt);
}

void
finalize_pass_jfunc ()
{
  while (!jfuncs.is_empty ())
    free (jfuncs.pop ());
}

