/* Copyright (C) 2017 Vladislav Ivanishin */

#include "gcc-plugin.h"
#include "tree.h"
#include "tree-pass.h"
#include "plugin-version.h"
#include "context.h"
#include "cgraph.h"

int plugin_is_GPL_compatible;

namespace {

const pass_data pass_data_hide_globally_invisible =
{
  SIMPLE_IPA_PASS, /* type */
  "hide-globally-invisible", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_NONE, /* tv_id */
  0, /* properties_required */ // TODO: PROP_cfg ?
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  0, /* todo_flags_finish */
};

class pass_hide_globally_invisible : public simple_ipa_opt_pass
{
public:
  pass_hide_globally_invisible (gcc::context *ctxt)
    : simple_ipa_opt_pass (pass_data_hide_globally_invisible, ctxt)
  {}

  virtual unsigned int execute (function *);

}; // class pass_hide_globally_invisible

static const char *
decl_name (cgraph_node *node)
{
  return IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (node->decl));
}

static bool
no_external_uses_p (cgraph_node *node)
{
  return !strcmp (decl_name (node), "staticfoo");
}

static bool
lib_private_p (cgraph_node *node)
{
  return !strcmp (decl_name (node), "libprivate");
}

unsigned int
pass_hide_globally_invisible::execute (function *)
{
  cgraph_node *node;

  FOR_EACH_FUNCTION (node)
    {
      if (no_external_uses_p (node))
	{
	  gcc_assert (node->decl);
	  /* This causes cgraph_externally_visible_p to return FALSE, which leads
	     to localize_node being called from function_and_variable_visibility
	     (ipa-visibility aka "visibility" pass).  */
	  TREE_PUBLIC (node->decl) = 0;
	}
      if (lib_private_p (node))
	{
	  gcc_assert (node->decl);
	  DECL_VISIBILITY (node->decl) = VISIBILITY_HIDDEN;
	  DECL_VISIBILITY_SPECIFIED (node->decl) = true;
	}
    }

  return 0;
}

} // anon namespace

static simple_ipa_opt_pass *
make_pass_hide_globally_invisible (gcc::context *ctxt)
{
  return new pass_hide_globally_invisible (ctxt);
}

int
plugin_init (plugin_name_args *i, plugin_gcc_version *v)
{
  plugin_init_func check = plugin_init;
  struct register_pass_info pass_info;

  if (!plugin_default_version_check (&gcc_version, v))
    {
      fprintf (stderr, "GCC version does not match plugin's.");
      return 1;
    }

  pass_info.pass = make_pass_hide_globally_invisible (g);
  pass_info.reference_pass_name = "visibility";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_BEFORE;

  register_callback (i->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_info);

  return 0;
}
