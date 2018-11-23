// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#ifndef SYMBOLS_PASS_H_INCLUDED
#define SYMBOLS_PASS_H_INCLUDED

typedef enum
{
  UNDEFINED,
  DYNAMIC,
  CONSTANT,
  PARTIALLY_CONSTANT
} resolve_lattice_t;

struct signature
{
  const char *func_name;
  unsigned sym_pos;
};

/* Describes the context of particular function */
struct call_info
{
  /* Function node in the call graph */
  struct cgraph_node *node;
  /* Call statement */
  gimple stmt;
  /* Signature of dynamic call */
  struct signature *sign;
};

/* Describes the context of dlsym usage */
struct resolve_ctx
{
  /* Initial signature of dlsym */
  struct signature *base_sign;
  /* Caller node */
  struct cgraph_node *node;
  /* Call location */
  location_t loc;
  /* Resolve status */
  resolve_lattice_t status;

  /* Call stack of symbols to detect recursions */
  vec<const char *> *considered_functions;
  /* List of possible symbols, keep only unique symbols */
  vec<const char *> *symbols;
  /* Chain of calls (wrappers) that pass symbol through body.
     In other words, callstack. */
  vec<call_info> *calls_chain;
};

simple_ipa_opt_pass *
make_pass_symbols (gcc::context *ctxt);
void
finalize_pass_symbols ();

void
dump_dynamic_symbol_calls (struct resolve_ctx *ctx);
void
dump_node (struct cgraph_node *node);
void
dump_lattice_value (FILE *outf, resolve_lattice_t val);
void
dump_decl_section (FILE *outf, tree decl);

#endif
