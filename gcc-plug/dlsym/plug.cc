#include "common.h"
#include "plugin-version.h"
#include "symbols-pass.h"

int plugin_is_GPL_compatible;

char md5str[33];
const char *output_fd = NULL;

extern vec<struct signature> signatures;
extern vec<struct resolve_ctx *> resolve_contexts;

void
write_dynamic_symbol_calls (FILE *outf, vec<struct resolve_ctx *> *ctxs);

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
      }
  fclose (f);

  unsigned char md5sum[16];
  md5_buffer (streamptr, streamsz, md5sum);
  free (streamptr);

  printmd5 (md5str, md5sum);
}

void
plugin_finalize (void *, void *)
{
  if (output_fd)
    {
      FILE *output = fdopen (atoi (output_fd), "w");
      if (!output)
	fatal_error (xstrerror (errno));

      write_dynamic_symbol_calls (output, &resolve_contexts);

      fclose (output);
    }

  finalize_pass_symbols ();
}

void static
parse_argument (plugin_argument *arg)
{
  const char *arg_key = arg->key;
  const char *arg_value = arg->value;
  /* Read dynamic caller signature. For instance:
     -fplugin-libplug-sign-dlsym=1
     means that name of caller is 'dlsym' and symbol is an argument no. 1. */
  if (!strncmp (arg_key, "sign-", 5))
    {
      struct signature initial = { xstrdup (&arg_key[5]), atoi (arg_value) };
      signatures.safe_push (initial);
      return;
    }

  /* Read a file descriptor to output symbols:
     -fplugin-libplug-out=1 */
  if (!strcmp(arg_key, "out"))
    {
      output_fd = arg_value;
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
  pass_info.pass = make_pass_symbols (g);
  pass_info.reference_pass_name = "simdclone";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_BEFORE;

  register_callback (plugin_info->base_name,
		     PLUGIN_ALL_IPA_PASSES_START, compute_md5,
		     NULL);
  register_callback (plugin_info->base_name,
		     PLUGIN_PASS_MANAGER_SETUP, NULL,
		     &pass_info);
  register_callback (plugin_info->base_name,
		     PLUGIN_FINISH, plugin_finalize,
		     NULL);
  return 0;
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


