// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#ifndef JFUNC_PASS_H_INCLUDED
#define JFUNC_PASS_H_INCLUDED

struct jfunction
{
  const char *from_name;
  unsigned from_arg;
  const char *to_name;
  unsigned to_arg;
};

simple_ipa_opt_pass *make_pass_jfunc (gcc::context *ctxt);
void finalize_pass_jfunc ();

#endif
