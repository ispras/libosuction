// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct jfunction
{
  const char *from_name;
  int from_arg;
  const char *to_name;
  int to_arg;
};

struct list_node
{
  struct jfunction *jf;
  struct list_node *next;
};

static bool
has_duplicates(struct jfunction *jf, struct list_node *list)
{
  struct list_node *iter;
  for (iter = list; iter; iter = iter->next)
    if (!strcmp(jf->from_name, iter->jf->from_name)
	&& jf->from_arg == iter->jf->from_arg)
      return true;
  return false;
}

static struct list_node *
create_list_node(const char *from_name, int from_arg,
		 const char *to_name, int to_arg,
		 struct list_node *next)
{
  struct list_node *node = calloc(1, sizeof *node);
  node->jf = calloc(1, sizeof *node->jf);
  node->jf->from_name = from_name;
  node->jf->from_arg = from_arg;
  node->jf->to_name = to_name;
  node->jf->to_arg = to_arg;
  node->next = next;
  return node;
}

static struct list_node *
read_jf(FILE *f)
{
  struct list_node *res = NULL;
  char *from_name, *to_name;
  int from_arg, to_arg;
  while (fscanf(f, "%ms %d %ms %d", &from_name, &from_arg, &to_name, &to_arg) != EOF)
      res = create_list_node(from_name, from_arg, to_name, to_arg, res);
  return res;
}

static struct list_node *
read_base(FILE *f)
{
  struct list_node *res = NULL;
  char *from_name;
  int from_arg;
  while (fscanf(f, "%ms %d", &from_name, &from_arg) != EOF)
      res = create_list_node(from_name, from_arg, NULL, 0, res);
  return res;
}

static struct list_node *
find_closure(struct list_node *start, struct list_node *pool)
{
  struct list_node *current = start, *next = NULL, *output = NULL;
  while (current)
    {
      struct list_node *temp;
      for (temp = pool; temp; temp = temp->next)
	{
	  struct list_node *cur_iter;
	  for (cur_iter = current; cur_iter; cur_iter = cur_iter->next)
	    {
	      if (!strcmp (temp->jf->to_name, cur_iter->jf->from_name)
		  && temp->jf->to_arg == cur_iter->jf->from_arg)
		{
		  /* Check duplicates in three disjoint lists */
		  if (has_duplicates(temp->jf, next)
		      || has_duplicates(temp->jf, output)
		      || has_duplicates(temp->jf, current))
		    break;

		  // Include jf to dlsym signatures
		  struct list_node *nw = calloc(1, sizeof *nw);
		  nw->jf = temp->jf;
		  nw->next = next;
		  next = nw;
		  break;
		}
	    }
	}

      /* Signatures from CURRENT move to the beginning of OUTPUT;
	 signatures from NEXT move to CURRENT.
	 If NEXT is empty, therefore we did not find anything, stop the search
	 else set NEXT to zero and continue, because list of signatures has
	 been updated. */
      if (output)
	{
	  struct list_node *last = current;
	  while (last->next)
	    last = last->next;
	  last->next = output;
	}
      output = current;
      current = next;
      next = NULL;
    }
  return output;
}

int main (int argc, char *argv[])
{
  struct list_node *base_dlsym, *root, *output;
  FILE *f;

  if (argc < 3)
    {
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

  for (struct list_node *iter = output; iter; iter = iter->next)
    fprintf(outf, "%s %d\n", iter->jf->from_name, iter->jf->from_arg);

  if (argc >= 4)
    fclose(outf);

  // Free memory
  for (struct list_node *iter = root, *tmp; iter; iter = tmp) {
    tmp = iter->next;
    free(iter->jf);
    free(iter);
  }

  // All jfuncs are already freed
  for (struct list_node *iter = output, *tmp; iter; iter = tmp) {
    tmp = iter->next;
    free(iter);
  }

  return 0;
}

