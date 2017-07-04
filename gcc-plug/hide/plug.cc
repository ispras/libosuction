/* Copyright (C) 2017 Vladislav Ivanishin */

#include "bversion.h"
#include "gcc-plugin.h"
#include "tree.h"
#include "tree-pass.h"
#include "plugin-version.h"
#include "context.h"
#include "cgraph.h"
#include "splay-tree.h"
#include "diagnostic-core.h"

#if BUILDING_GCC_VERSION < 4009
#error "This gcc is too old. The minimum required version is 4.9.x"
#endif

int plugin_is_GPL_compatible;

extern const char *user_label_prefix;

namespace {

const pass_data pass_data_hide_globally_invisible =
{
  SIMPLE_IPA_PASS, /* type */
  "hide-globally-invisible", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
#if BUILDING_GCC_VERSION == 4009
  false, /* has_gate */
  true, /* has_execute */
#endif
  TV_NONE, /* tv_id */
  0, /* properties_required */ // TODO: PROP_cfg ?
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  0, /* todo_flags_finish */
};

#if BUILDING_GCC_VERSION >= 6000
#define transparent_alias_p(node) node->transparent_alias
#else
#define transparent_alias_p(node) false
#endif

#if BUILDING_GCC_VERSION >= 5000
#define _EXECUTE_ARGS function *
#define fatal_error(...) fatal_error (UNKNOWN_LOCATION, __VA_ARGS__)
#define set_comdat_group(node, group) node->set_comdat_group (group)
#define set_section(node, section) node->set_section (section)
#define make_decl_local(node) node->make_decl_local ()
#define dissolve_same_comdat_group_list(node) \
  node->dissolve_same_comdat_group_list ()
#else
#define _EXECUTE_ARGS void
#define fatal_error(...) fatal_error (__VA_ARGS__)
#define set_comdat_group(node, group) DECL_COMDAT_GROUP (node->decl) = group
#define set_section(node, section)
#define make_decl_local(node) symtab_make_decl_local (node->decl)
#define dissolve_same_comdat_group_list(node) \
  symtab_dissolve_same_comdat_group_list (node)
#endif


class pass_hide_globally_invisible : public simple_ipa_opt_pass
{
public:
  pass_hide_globally_invisible (gcc::context *ctxt)
    : simple_ipa_opt_pass (pass_data_hide_globally_invisible, ctxt),
    fname (NULL)
  {}

  virtual unsigned int execute (_EXECUTE_ARGS);

  void set_fname (const char *fname_) { fname = fname_; }

private:
  bool no_external_uses_p (symtab_node *node);
  bool lib_private_p (symtab_node *node);
  void read_vis_changes (void);
  bool localize_comdat (symtab_node *node);

  const char *fname; /* File to read visibility modifications info from.  */
  splay_tree static_nodes;
  splay_tree libprivate_nodes;
}; // class pass_hide_globally_invisible

static const char *
decl_name (const symtab_node *node)
{
  const char *asname = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (node->decl));
  if (asname[0] == '*')
    asname++;
  return asname;
}

/* Name of the object file we'll be emitting to.  The caller is responsible for
   freeing the memory.  */
const char *
cur_filename_base ()
{
  const char *ofilename = strrchr (main_input_filename, '/');
  if (!ofilename)
    ofilename = main_input_filename;
  else
    ofilename++;

  char *prefix = xstrdup (ofilename);

  /* FIXME: this is a hack (we are making an assumption).  */
  char *dot = strrchr (prefix , '.');
  gcc_assert (dot && strlen (ofilename) - (dot - ofilename + 1) >=  1);
  dot[1] = 'o';
  dot[2] = '\0';

  return prefix;
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
  char *symname, *prefix;
  int i;
  char c;

  /* This does not change during a plugin invocation.  */
  const char *cur_prefix = cur_filename_base ();

  for (i = 0; i < num_nodes; i++)
    {
      if (fscanf (f, "%*[\n \t]%m[^:]%c%ms", &prefix, &c, &symname) != 3
	  || c != ':')
	fatal_error ("error reading static/libprivate symbol names");

      if (!strcmp (cur_prefix, prefix))
	  splay_tree_insert (nodes, (splay_tree_key) symname, 0);

      free (prefix);
    }

  free ((char*) cur_prefix);
}

void
pass_hide_globally_invisible::read_vis_changes (void)
{
  FILE *f;
  int num_statics, num_libpriv;

  f = fopen (fname, "r");
  if (!f)
    fatal_error ("cannot open %s", fname);

  fscanf (f, "%d %d", &num_statics, &num_libpriv);
  read_decl_names (f, num_statics, static_nodes);
  read_decl_names (f, num_libpriv, libprivate_nodes);

  fclose (f);
}

/* Tries to localize the comdat group NODE belongs to, returns TRUE on success.
   This function resembles localize_node () from ipa-visibility.  */
bool
pass_hide_globally_invisible::localize_comdat (symtab_node *node)
{
  gcc_assert (node->same_comdat_group);

  symtab_node *next, *cur;

  for (next = node->same_comdat_group;
       next != node; next = next->same_comdat_group)
    if (!no_external_uses_p (next))
      return false;

  cur = node;
  do
    {
      next = cur->same_comdat_group;
      set_comdat_group (cur, NULL_TREE);
      if (!cur->alias)
	set_section (cur, NULL);
      if (!transparent_alias_p (cur))
	make_decl_local (cur);
      cur = next;
    }
  while (cur != node);

  dissolve_same_comdat_group_list (node);

  return true;
}

/* The compiler segfaults if we change visibility of these functions.  It should
   be enough to special-case them.  */
static bool
dont_hide_p (const symtab_node *node)
{
  return (!strcmp (decl_name (node), "__cxa_pure_virtual")
	  || !strcmp (decl_name (node), "__cxa_deleted_virtual"));
}

unsigned int
pass_hide_globally_invisible::execute (_EXECUTE_ARGS)
{
  symtab_node *node;
  bool comdat_priv_failed_p;

  static_nodes = splay_tree_new ((splay_tree_compare_fn) strcmp,
				 (splay_tree_delete_key_fn) free,
				 0);
  libprivate_nodes = splay_tree_new ((splay_tree_compare_fn) strcmp,
				     (splay_tree_delete_key_fn) free,
				     0);
  read_vis_changes ();

  FOR_EACH_SYMBOL (node)
    {
      comdat_priv_failed_p = false;

      if (no_external_uses_p (node))
	{
	  gcc_assert (!transparent_alias_p (node));

          if (!TREE_PUBLIC (node->decl))
            continue;

	  /* If all members of a comdat are known to be static, make them local
	     and dissolve this comdat group.  Otherwise the deal is off, only
	     adjust the visibility of this node and keep the comdat.  */
          if (node->same_comdat_group)
            comdat_priv_failed_p = !localize_comdat (node);
	  else
	    make_decl_local (node);
	}
      if ((lib_private_p (node) || comdat_priv_failed_p)
	  && !dont_hide_p (node))
	{
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

  if (user_label_prefix && strlen (user_label_prefix))
    fatal_error ("the plugin does not make provision for -fleading-underscore");

  char *fname = NULL;
  for (arg = i->argv; arg; arg++)
    if (!strcmp (arg->key, "fname"))
      {
	fname = arg->value;
	break;
      }
  if (!fname)
    fatal_error ("file with visibility modifications not specified "
		 "(pass 'fname' argument to the plugin)");

  pass_info.pass = make_pass_hide_globally_invisible (g, fname);
  pass_info.reference_pass_name = "visibility";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_BEFORE;

  register_callback (i->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_info);

  return 0;
}
