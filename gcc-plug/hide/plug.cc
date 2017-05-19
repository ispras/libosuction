/* Copyright (C) 2017 Vladislav Ivanishin */

#include "gcc-plugin.h"
#include "tree.h"
#include "tree-pass.h"
#include "plugin-version.h"
#include "context.h"
#include "cgraph.h"

int plugin_is_GPL_compatible;

namespace {

const pass_data pass_data_drop_globally_unused =
{
  SIMPLE_IPA_PASS, /* type */
  "drop-globally-unused", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_NONE, /* tv_id */
  0, /* properties_required */ // TODO: PROP_cfg ?
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  0, /* todo_flags_finish */
};

class pass_drop_globally_unused : public simple_ipa_opt_pass
{
public:
  pass_drop_globally_unused (gcc::context *ctxt)
    : simple_ipa_opt_pass (pass_data_drop_globally_unused, ctxt)
  {}

  virtual unsigned int execute (function *);

}; // class pass_drop_globally_unused

static bool
unused_p (cgraph_node *node)
{
  return !strcmp (node->name (), "foo");
}

unsigned int
pass_drop_globally_unused::execute (function *)
{
  cgraph_node *node;
  cgraph_node *next;

  for (node = symtab->first_function (); node; node = next)
    {
      next = symtab->next_function (node);

      if (unused_p (node))
	node->remove ();
      else
	printf ("%s\n", node->name ());
    }
  return 0;
}

} // anon namespace

static simple_ipa_opt_pass *
make_pass_drop_globally_unused (gcc::context *ctxt)
{
  return new pass_drop_globally_unused (ctxt);
}

int
plugin_init (plugin_name_args *i, plugin_gcc_version *v)
{
  plugin_init_func check = plugin_init;
  struct register_pass_info pass_info;

  if (!plugin_default_version_check (&gcc_version ,v))
    {
      fprintf (stderr, "GCC version does not match plugin's.");
      return 1;
    }

  pass_info.pass = make_pass_drop_globally_unused (g);
  pass_info.reference_pass_name = "materialize-all-clones";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback (i->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_info);
  return 0;
}
