// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#include <stdio.h>
#include "deps-graph.h"

int main(int argc, char *argv[])
{
	struct dso dsos[argc-2];
	for (int i = 1; i < argc; i++) {
		FILE *f = fopen(argv[i], "r");
		if (!f) return 1;
		if (i == 1)
			input_dyndeps(f);
		else
			input(dsos+i-2, f);
		fclose(f);
	}
	//scc(dsos); return 0;
	merge(dsos, argc-2);

	for (struct dso *dso = dsos; dso < dsos + argc - 2; dso++)
		printmark(stdout, dso);
	return 0;
}

