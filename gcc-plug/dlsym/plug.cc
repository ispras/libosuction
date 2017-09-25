#include "common.h"
#include "plugin-version.h"
#include "symbols-pass.h"
#include "jfunc-pass.h"

int plugin_is_GPL_compatible;

char md5str[33];
const char *sym_out_fd = NULL;
const char *jf_out_fd = NULL;
int run = 1;

extern vec<struct signature> signatures;
extern vec<struct resolve_ctx *> resolve_contexts;
extern vec<struct jfunction *> jfuncs;

void
printmd5 (char *p, const unsigned char md5[])
{
  for (int i = 0; i < 16; i++)
    {
      *p++ = "0123456789abcdef"[md5[i] >> 4];
      *p++ = "0123456789abcdef"[md5[i] & 15];
    }
  *p = '\0';
}

void
compute_md5 (void *, void *)
{
  /* Do not compute md5 hash due to absense the body */
  if (in_lto_p)
    return;

  char *streamptr;
  size_t streamsz;
  FILE *f = open_memstream (&streamptr, &streamsz);

  int save_noaddr = flag_dump_noaddr;
  flag_dump_noaddr = 1;
  dump_symtab (f);

  cgraph_node *node;
  FOR_EACH_DEFINED_FUNCTION (node)
    if (cgraph_function_with_gimple_body_p (node))
      dump_function_to_file (node->decl, f, TDF_SLIM);

  flag_dump_noaddr = save_noaddr;

  unsigned i;
  for (i = 0; i < save_decoded_options_count; i++)
    switch (save_decoded_options[i].opt_index)
      {
      default:
	fprintf(f, "%s\n",
		save_decoded_options[i].orig_option_with_args_text);
      case OPT_o:;
      case OPT_fplugin_:;
      case OPT_fplugin_arg_:;
      case OPT_flto_:;
      case OPT_flto:;
      case OPT_ffunction_sections:;
      case OPT_fdata_sections:;
      }
  fclose (f);

  unsigned char md5sum[16];
  md5_buffer (streamptr, streamsz, md5sum);
  free (streamptr);

  printmd5 (md5str, md5sum);
}

void
write_dynamic_symbol_calls (FILE *output, vec<struct resolve_ctx *> *ctxs)
{
  unsigned i, j;
  const char *symbol;
  struct resolve_ctx *ctx;

  for (i = 0; ctxs->iterate (i, &ctx); ++i)
    {
      gcc_assert (ctx->node);
      gcc_assert (ctx->node->decl);
      /* Output the header in the following format:
	 "<file>:<line>:<srcid>:<section>:<caller>:<status>:" */
      fprintf (output, "%s:%d:%s:", LOCATION_FILE (ctx->loc),
	       LOCATION_LINE (ctx->loc), md5str);
      dump_decl_section (output, ctx->node->decl);
      fprintf (output, ":%s:", assemble_name_raw (ctx->node));
      dump_lattice_value (output, ctx->status);
      fprintf (output, ":");

      /* Output symbols separated by comma */
      for (j = 0; ctx->symbols->iterate (j, &symbol); ++j)
	{
	  if (j)
	    fprintf (output, ",");
	  fprintf (output, "%s", symbol);
	}
      fprintf(output, "\n");
    }
}

void
write_jfunctions (FILE *output, vec<struct jfunction *> *jfs)
{
  unsigned i;
  struct jfunction *jf;

  for (i = 0; jfs->iterate (i, &jf); ++i)
    fprintf (output, "%s %d %s %d\n",
	     jf->from_name, jf->from_arg,
	     jf->to_name, jf->to_arg);
}

void
plugin_finalize (void *, void *)
{
  if (run == 0)
    {
      if (jf_out_fd)
	{
	  FILE *output = fdopen (atoi (jf_out_fd), "a");
	  if (!output)
	    fatal_error (xstrerror (errno));

	  write_jfunctions (output, &jfuncs);

	  fclose (output);
	}
      finalize_pass_jfunc ();
    }

  if (run == 1)
    {
      if (sym_out_fd)
	{
	  FILE *output = fdopen (atoi (sym_out_fd), "w");
	  if (!output)
	    fatal_error (xstrerror (errno));

	  write_dynamic_symbol_calls (output, &resolve_contexts);

	  fclose (output);
	}
      finalize_pass_symbols ();
    }
}

void static
parse_argument (plugin_argument *arg)
{
  const char *arg_key = arg->key;
  const char *arg_value = arg->value;
  /* Read dynamic caller signature. For instance:
     -fplugin-libplug-sign-dlsym=1
     means that name of caller is 'dlsym' and symbol is an argument no. 1. */
  if (!strncmp (arg_key, "sign-", 5) && arg_key[5] != '\0')
    {
      struct signature initial = { xstrdup (&arg_key[5]), atoi (arg_value) };
      signatures.safe_push (initial);
      return;
    }

  /* Read a file descriptor to output symbols:
     -fplugin-libplug-symout=1 */
  if (!strcmp(arg_key, "symout"))
    {
      sym_out_fd = arg_value;
      return;
    }

  /* Read a file descriptor to output jump functions:
     -fplugin-libplug-jfout=2 */
  if (!strcmp(arg_key, "jfout"))
    {
      jf_out_fd = arg_value;
      return;
    }

  /* Read the signatures from an input file:
     -fplugin-libplug-in=signs.txt
     in format:
     <signature name> <symbol position> */
  if (!strcmp(arg_key, "in"))
    {
      int spos;
      const char *wname;
      FILE *input = fopen (arg_value, "r");

      if (!input)
	fatal_error (xstrerror (errno));

      while (fscanf (input, "%ms %d", &wname, &spos) != EOF)
	{
	  struct signature sign = { wname, spos };
	  signatures.safe_push (sign);
	}

      fclose (input);
      return;
    }

  /* Read the mode of plugin:
     -fplugin-libplug-run={0,1}
     0 -- perform jump functions pass
     1 -- perform symbols pass */
  if (!strcmp (arg_key, "run"))
    {
      if ((arg_value[0] == '0' || arg_value[0] == '1')
	  && arg_value[1] == '\0')
	run = arg_value[0] - '0';
      else
	fatal_error ("run parameter accepts only 0 or 1");
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
  gcc_version.configuration_arguments = version->configuration_arguments;

  if (!plugin_default_version_check (&gcc_version ,version))
    return 1;

  parse_arguments (plugin_info->argc, plugin_info->argv);

  register_callback (plugin_info->base_name,
		     PLUGIN_ALL_IPA_PASSES_START, compute_md5,
		     NULL);

  if (run == 0)
    {
      struct register_pass_info pass_info_jfunc;
      pass_info_jfunc.pass = make_pass_jfunc (g);
      pass_info_jfunc.reference_pass_name = "simdclone";
      pass_info_jfunc.ref_pass_instance_number = 1;
      pass_info_jfunc.pos_op = PASS_POS_INSERT_BEFORE;

      register_callback (plugin_info->base_name,
			 PLUGIN_PASS_MANAGER_SETUP, NULL,
			 &pass_info_jfunc);
    }

  if (run == 1)
    {
      struct register_pass_info pass_info_symbols;
      pass_info_symbols.pass = make_pass_symbols (g);
      pass_info_symbols.reference_pass_name = "simdclone";
      pass_info_symbols.ref_pass_instance_number = 1;
      pass_info_symbols.pos_op = PASS_POS_INSERT_BEFORE;

      register_callback (plugin_info->base_name,
			 PLUGIN_PASS_MANAGER_SETUP, NULL,
			 &pass_info_symbols);
    }

  register_callback (plugin_info->base_name,
		     PLUGIN_FINISH, plugin_finalize,
		     NULL);
  return 0;
}

