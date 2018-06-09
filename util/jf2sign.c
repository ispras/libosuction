// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#include <stdio.h>
#include <stdlib.h>

#include "jfuncs.h"

int main (int argc, char *argv[])
{
  struct jfnode *base_dlsym, *root, *output;
  FILE *f;

  if (argc < 3) {
    fprintf(stderr, "Two required arguments should be given:\n"
		    " 1) a file with jump functions\n"
		    " 2) a file with base signatures\n"
		    " 3) [Optional] a file to output resulting signatures.\n");
    return 1;
  }

  // Read jump functions
  f = fopen(argv[1], "r");
  if (!f) return 1;
  root = read_jf(f);
  fclose(f);

  // Read initial calls
  f = fopen(argv[2], "r");
  if (!f) return 1;
  base_dlsym = read_base(f);
  fclose(f);

  // Match jump function chains
  output = find_closure(base_dlsym, root);

  // Output results
  FILE *outf = argc < 4 ? stdout : fopen(argv[3], "w");

  print_jflist(outf, output);

  if (argc >= 4)
    fclose(outf);

  free_jflist(&root);
  free_jflist(&output);

  return 0;
}

