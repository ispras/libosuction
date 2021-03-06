// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#include <cstdio>

#include "bversion.h"
#include "gcc-plugin.h"
#include "opts.h"
#include "toplev.h"
#include "tree.h"
#include "output.h"
#include "tree-cfg.h"
#include "tree-pass.h"
#include "plugin-version.h"
#include "context.h"
#include "cgraph.h"
#include "splay-tree.h"
#include "diagnostic-core.h"
#include "md5.h"

#include "gcc-plug/common.h"

#if BUILDING_GCC_VERSION < 4009
#error "This gcc is too old. The minimum required version is 4.9.x"
#endif

int plugin_is_GPL_compatible;

namespace {

static char md5str[32];

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
#define asm_nodes symtab->first_asm_symbol ()
#define add_asm_node(asm_str) symtab->finalize_toplevel_asm (asm_str)
#if BUILDING_GCC_VERSION >= 8001
#define dump_symtab(f) symtab->dump (f)
#else
#define dump_symtab(f) symtab_node::dump_table (f)
#endif
#define cgraph_function_with_gimple_body_p(node) node->has_gimple_body_p ()
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
    fname (NULL), sockfd (-1)
  {}

  virtual unsigned int execute (_EXECUTE_ARGS);

  void set_fname (const char *fname_) { fname = fname_; }
  void set_sockfd (const int sockfd_) { sockfd = sockfd_; }

private:
  bool eliminate_p (symtab_node *node);
  bool localize_p (symtab_node *node);
  bool hide_p (symtab_node *node);
  void read_vis_changes (void);
  bool localize_comdat (symtab_node *node);
  void do_localize_node (symtab_node *node);

  const char *fname; /* File to read visibility modifications info from.  */
  int sockfd; /* Socket descriptor to interact with daemon.  */
  splay_tree elim_nodes;
  splay_tree loc_nodes;
  splay_tree hid_nodes;
}; // class pass_hide_globally_invisible

const char *
decl_name (const symtab_node *node)
{
  const char *asname = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (node->decl));
  if (asname[0] == '*')
    asname++;
  return asname;
}

bool
pass_hide_globally_invisible::eliminate_p (symtab_node *node)
{
  return !!splay_tree_lookup (elim_nodes, (splay_tree_key) decl_name (node));
}

bool
pass_hide_globally_invisible::localize_p (symtab_node *node)
{
  return !!splay_tree_lookup (loc_nodes, (splay_tree_key) decl_name (node));
}

bool
pass_hide_globally_invisible::hide_p (symtab_node *node)
{
  return !!splay_tree_lookup (hid_nodes, (splay_tree_key) decl_name (node));
}

void
read_decl_names (FILE *f, int num_nodes, splay_tree nodes)
{
  char *symname;
  char srcid[32 + 1];
  int i;
  char c1 = 0, c2 = 0;

  for (i = 0; i < num_nodes; i++)
    {
      if ((fscanf (f, "%*[\n \t]%*[^:]%c%32[^:]%c%ms",
                   &c1, &srcid, &c2, &symname) != 4)
          || c1 != ':' || c2 != ':')
        fatal_error ("error reading elim/loc/hid symbol names");

      if (!strncmp (md5str, srcid, 32))
	splay_tree_insert (nodes, (splay_tree_key) symname, 0);
    }
}

void
pass_hide_globally_invisible::read_vis_changes (void)
{
  FILE *fin, *fout;
  int nelim, nloc, nhid;

  if (sockfd >= 0) {
    if (!(fout = fdopen(sockfd, "w"))
	|| fprintf(fout, "%.32s", md5str) < 0
	|| fflush(fout) != 0)
      fatal_error ("cannot open sockfd %d to write: %s", sockfd, xstrerror (errno));

    fin = fdopen(dup(sockfd), "r");
    if (!fin)
      fatal_error ("cannot open sockfd %d to read: %s", sockfd, xstrerror (errno));

    fclose (fout);
  } else if (fname) {
    fin = fopen (fname, "r");
    if (!fin)
      fatal_error ("cannot open %s", fname);
  } else {
      fatal_error ("no input file");
  }

  fscanf (fin, "%d %d %d", &nelim, &nloc, &nhid);

  read_decl_names (fin, nelim, elim_nodes);

  /*  Act conservatively if there can possibly be an alias defined via
      `.symver`; if we localize the symbol such an alias will become local
      as well.  We can't even make it hidden for the same reason.  */
  if (!asm_nodes)
    {
      read_decl_names (fin, nloc, loc_nodes);
      read_decl_names (fin, nhid, hid_nodes);
    }

  fclose (fin);
}

void
pass_hide_globally_invisible::do_localize_node (symtab_node *node)
{
  make_decl_local (node);
  TREE_USED (node->decl) = 1;
  DECL_PRESERVE_P (node->decl) = 1;
  node->force_output = 1;

  /* This code does the right thing but looks useless since control never
     reaches inside the if.  Theoretical use-cases are
     a. We have loc symbols, but no toplevel asms (symbols are local not due to
        being in the same section as their .symver alias);
     b. A comdat group member which is only local (while some other member of
        the same comdat is eliminable).  */
#if 0
  if (localize_p (node))
    {
      TREE_USED (node->decl) = 1;
    }
#endif
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
    if (!(eliminate_p (next) || localize_p (next)))
      return false;

  cur = node;
  do
    {
      next = cur->same_comdat_group;
      set_comdat_group (cur, NULL_TREE);
      if (!cur->alias)
	set_section (cur, NULL);
      if (!transparent_alias_p (cur))
	do_localize_node (cur);
      cur = next;
    }
  while (cur != node);

  dissolve_same_comdat_group_list (node);

  return true;
}

/* The compiler segfaults if we change visibility of these functions.  It should
   be enough to special-case them.  */
bool
dont_hide_p (const symtab_node *node)
{
  return (!strcmp (decl_name (node), "__cxa_pure_virtual")
	  || !strcmp (decl_name (node), "__cxa_deleted_virtual"));
}

unsigned int
pass_hide_globally_invisible::execute (_EXECUTE_ARGS)
{
  symtab_node *node;
  bool priv_failed_p;

  elim_nodes = splay_tree_new ((splay_tree_compare_fn) strcmp,
                               (splay_tree_delete_key_fn) free,
                               0);
  loc_nodes = splay_tree_new ((splay_tree_compare_fn) strcmp,
                              (splay_tree_delete_key_fn) free,
                              0);
  hid_nodes = splay_tree_new ((splay_tree_compare_fn) strcmp,
                              (splay_tree_delete_key_fn) free,
                              0);
  read_vis_changes ();

  FOR_EACH_SYMBOL (node)
    {
      priv_failed_p = false;

      if (localize_p (node) || eliminate_p (node))
	{
	  gcc_assert (!transparent_alias_p (node));

          if (!TREE_PUBLIC (node->decl))
            continue;

	  /*  TODO: this is a temp fix.  The semantics we want resembles that of
	      "inline".  Ideally the symbol should remain weak but the compiler
	      should be able to eliminate it (we know that there are no
	      references to this symbol from outside this TU but GCC does not
	      allow weak non-public symbols).  */
	  if (DECL_WEAK (node->decl))
	    {
	      priv_failed_p = true;
	      goto hide;
	    }

	  /* If all members of a comdat are known to be static, make them local
	     and dissolve this comdat group.  Otherwise the deal is off, only
	     adjust the visibility of this node and keep the comdat.  */
          if (node->same_comdat_group)
            priv_failed_p = !localize_comdat (node);
	  else
	    do_localize_node (node);
	}
    hide:
      if ((hide_p (node) || priv_failed_p) && !dont_hide_p (node))
	{
	  DECL_VISIBILITY (node->decl) = VISIBILITY_HIDDEN;
	  DECL_VISIBILITY_SPECIFIED (node->decl) = true;
	}
    }

  splay_tree_delete (elim_nodes);
  splay_tree_delete (loc_nodes);
  splay_tree_delete (hid_nodes);
  return 0;
}

simple_ipa_opt_pass *
make_pass_hide_globally_invisible (gcc::context *ctxt, const char *filename,
				   const int sockfd)
{
  pass_hide_globally_invisible *pass = new pass_hide_globally_invisible (ctxt);
  pass->set_fname (filename);
  pass->set_sockfd (sockfd);
  return pass;
}

#define PLUG_SECTION_PREFIX ".comment.privplugid."

void
printmd5 (char *p, const unsigned char md5[])
{
  for (int i = 0; i < 16; i++)
    {
      *p++ = "0123456789abcdef"[md5[i] >> 4];
      *p++ = "0123456789abcdef"[md5[i] & 15];
    }
}

void
emit_privplugid_section (void *, void *)
{
  char buf[] =
   "\t.pushsection\t" PLUG_SECTION_PREFIX
   "\0_23456789abcdef0123456789abcdef,\"e\",%note\n"
   "\t.popsection\n";
  strncpy (buf + strlen (buf), md5str, 32);
  fprintf (asm_out_file, "%s", buf);
}

void
compmd5 (void *, void *)
{
  /* Do not compute md5 hash due to the absense of the body.  */
  if (in_lto_p)
    return;

  char *streamptr;
  size_t streamsz;
  FILE *f = open_memstream (&streamptr, &streamsz);

  int save_noaddr = flag_dump_noaddr;
  flag_dump_noaddr = 1;
  dump_symtab (f);

  flag_dump_noaddr = save_noaddr;

  for (size_t i = 0; i < save_decoded_options_count; i++)
    switch (save_decoded_options[i].opt_index)
      {
      default:
	fprintf(f, "%s\n", save_decoded_options[i].orig_option_with_args_text);
      case OPT_o:;
      case OPT_fplugin_:;
      case OPT_fplugin_arg_:;
      case OPT_flto_:;
      case OPT_flto:;
      case OPT_ffunction_sections:;
      case OPT_fdata_sections:;
      case OPT_SPECIAL_input_file:;
      case OPT_dumpbase:;
      case OPT_dumpdir:;
      case OPT_auxbase:;
      case OPT_auxbase_strip:;
      case OPT_I:;
      case OPT_L:;
      case OPT_fuse_ld_bfd:;
      case OPT_fuse_ld_gold:;
      }
  fclose (f);

  streamsz = erase_strings (streamptr, streamsz);

  unsigned char md5sum[16];
  md5_buffer (streamptr, streamsz, md5sum);
  free (streamptr);

  printmd5 (md5str, md5sum);
}

} // anon namespace

int
plugin_init (plugin_name_args *i, plugin_gcc_version *v)
{
  plugin_argument *arg;
  struct register_pass_info pass_info;
  char *fname = NULL;
  int sockfd = -1;
  long gcc_run = 0;

  gcc_version.configuration_arguments = v->configuration_arguments;

  if (!plugin_default_version_check (&gcc_version, v))
    {
      fatal_error ("GCC version does not match plugin's.");
      return 1;
    }

  if (flag_ltrans || flag_wpa)
    return 0;

  if (user_label_prefix && strlen (user_label_prefix))
    fatal_error ("the plugin does not make provision for -fleading-underscore");

  for (arg = i->argv; arg < i->argv + i->argc; arg++)
    if (!strcmp (arg->key, "fname"))
      fname = arg->value;
    else if (!strcmp (arg->key, "sockfd"))
      sscanf (arg->value, "%d", &sockfd);
    else if (!strcmp (arg->key, "run"))
      {
        errno = 0;
        gcc_run = strtol (arg->value, (char **) NULL, 10);
        if (errno)
          goto bad_gcc_run;
      }
    else
      fatal_error ("unknown plugin option '%s'", arg->key);
  if (gcc_run == 2 && sockfd < 0 && !fname)
    fatal_error ("visibility modifications not specified "
		 "(pass 'sockfd' or 'fname' arguments to the plugin)");
  if (sockfd >= 0 && fname)
    fatal_error ("'sockfd' and 'fname' are not compatible "
		 "(pass only one of them)");
  if (!(gcc_run == 1 || gcc_run == 2))
  bad_gcc_run:
    fatal_error ("pass run = 1 or 2 to the plugin");

  pass_info.pass = make_pass_hide_globally_invisible (g, fname, sockfd);
  pass_info.reference_pass_name = "visibility";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_BEFORE;

  register_callback (i->base_name, PLUGIN_ALL_IPA_PASSES_START, compmd5, NULL);
  register_callback (i->base_name, PLUGIN_FINISH_UNIT, emit_privplugid_section,
                     NULL);
  if (gcc_run == 2)
    register_callback (i->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_info);

  return 0;
}
