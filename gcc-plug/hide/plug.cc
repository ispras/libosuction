/* Copyright (C) 2017 Vladislav Ivanishin */

#include "gcc-plugin.h"
#include "tree.h"
#include "tree-pass.h"
#include "plugin-version.h"
#include "context.h"
#include "cgraph.h"
#include "splay-tree.h"
#include "diagnostic-core.h"

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
    : simple_ipa_opt_pass (pass_data_hide_globally_invisible, ctxt),
    fname (NULL)
  {}

  virtual unsigned int execute (function *);

  void set_fname (const char *fname_) { fname = fname_; }

private:
  bool no_external_uses_p (symtab_node *node);
  bool lib_private_p (symtab_node *node);
  void read_vis_changes (void);

  const char *fname; /* File to read visibility modifications info from.  */
  splay_tree static_nodes;
  splay_tree libprivate_nodes;
}; // class pass_hide_globally_invisible

static const char *
decl_name (symtab_node *node)
{
  return IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (node->decl));
}


bool
pass_hide_globally_invisible::no_external_uses_p (symtab_node *node)
{
  return !!splay_tree_lookup (static_nodes,
			      (splay_tree_key) decl_name (node));
}

bool
pass_hide_globally_invisible::lib_private_p (symtab_node *node)
{
  return !!splay_tree_lookup (libprivate_nodes,
			      (splay_tree_key) decl_name (node));
}

 /* We assume funciton names in the file F are prefixed by their length.   */
static void
read_decl_names (FILE *f, int num_nodes, splay_tree nodes)
{
  char *decl_name = NULL;
  int i;

  for (i = 0; i < num_nodes; i++)
    {
      if (fscanf (f, "%ms", &decl_name) != 1)
	fatal_error (UNKNOWN_LOCATION,
		     "error reading static/libprivate symbol names");

      splay_tree_insert (nodes, (splay_tree_key) decl_name, 0);
    }
}

void
pass_hide_globally_invisible::read_vis_changes (void)
{
  FILE *f;
  int num_statics, num_libpriv;

  f = fopen (fname, "r");
  if (!f)
    fatal_error (UNKNOWN_LOCATION, "cannot open %s", fname);

  fscanf (f, "%d %d", &num_statics, &num_libpriv);
  read_decl_names (f, num_statics, static_nodes);
  read_decl_names (f, num_libpriv, libprivate_nodes);

  fclose (f);
}

unsigned int
pass_hide_globally_invisible::execute (function *)
{
  symtab_node *node;

  static_nodes = splay_tree_new ((splay_tree_compare_fn) strcmp,
				 (splay_tree_delete_key_fn) free,
				 0);
  libprivate_nodes = splay_tree_new ((splay_tree_compare_fn) strcmp,
				     (splay_tree_delete_key_fn) free,
				     0);
  read_vis_changes ();

  FOR_EACH_SYMBOL (node)
    {
      if (no_external_uses_p (node))
	{
	  gcc_assert (node->decl);
	  /* For functions this causes cgraph_externally_visible_p to return
	     FALSE, which leads to localize_node being called from
	     function_and_variable_visibility (ipa-visibility aka "visibility"
	     pass).  */
	  TREE_PUBLIC (node->decl) = 0;
	  /* Weak static symbols make no sense.  */
	  DECL_WEAK (node->decl) = 0;
	}
      if (lib_private_p (node))
	{
	  gcc_assert (node->decl);
	  DECL_VISIBILITY (node->decl) = VISIBILITY_HIDDEN;
	  DECL_VISIBILITY_SPECIFIED (node->decl) = true;
	}
    }

  splay_tree_delete (static_nodes);
  splay_tree_delete (libprivate_nodes);
  return 0;
}

} // anon namespace

static simple_ipa_opt_pass *
make_pass_hide_globally_invisible (gcc::context *ctxt, const char *filename)
{
  pass_hide_globally_invisible *pass = new pass_hide_globally_invisible (ctxt);
  pass->set_fname (filename);
  return pass;
}

int
plugin_init (plugin_name_args *i, plugin_gcc_version *v)
{
  plugin_argument *arg;
  plugin_init_func check = plugin_init;
  struct register_pass_info pass_info;

  if (!plugin_default_version_check (&gcc_version, v))
    {
      fprintf (stderr, "GCC version does not match plugin's.");
      return 1;
    }

  char *fname = NULL;
  for (arg = i->argv; arg; arg++)
    if (!strcmp (arg->key, "fname"))
      {
	fname = arg->value;
	break;
      }
  if (!fname)
    fatal_error (UNKNOWN_LOCATION,
		 "file with visibility modifications not specified"
		 "(pass 'fname' argument to the plugin)");

  pass_info.pass = make_pass_hide_globally_invisible (g, fname);
  pass_info.reference_pass_name = "visibility";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_BEFORE;

  register_callback (i->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_info);

  return 0;
}
