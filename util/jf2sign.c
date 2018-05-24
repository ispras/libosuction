// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 ISP RAS (http://ispras.ru/en)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct jfnode
{
  struct jfunction
  {
    const char *from_name;
    int from_arg;
    const char *to_name;
    int to_arg;
  } *jf;
  struct jfnode *next;
};

static bool
has_duplicates(struct jfunction *jf, struct jfnode *list)
{
  struct jfnode *iter;
  for (iter = list; iter; iter = iter->next)
    if (!strcmp(jf->from_name, iter->jf->from_name)
	&& jf->from_arg == iter->jf->from_arg)
      return true;
  return false;
}

static struct jfnode *
create_jfnode(const char *from_name, int from_arg,
		 const char *to_name, int to_arg,
		 struct jfnode *next)
{
  struct jfnode *node = calloc(1, sizeof *node);
  node->jf = calloc(1, sizeof *node->jf);
  node->jf->from_name = from_name;
  node->jf->from_arg = from_arg;
  node->jf->to_name = to_name;
  node->jf->to_arg = to_arg;
  node->next = next;
  return node;
}

static struct jfnode *
read_jf(FILE *f)
{
  struct jfnode *res = NULL;
  char *from_name, *to_name;
  int from_arg, to_arg;
  while (fscanf(f, "%ms %d %ms %d", &from_name, &from_arg, &to_name, &to_arg) != EOF)
      res = create_jfnode(from_name, from_arg, to_name, to_arg, res);
  return res;
}

static struct jfnode *
read_base(FILE *f)
{
  struct jfnode *res = NULL;
  char *from_name;
  int from_arg;
  while (fscanf(f, "%ms %d", &from_name, &from_arg) != EOF)
      res = create_jfnode(from_name, from_arg, NULL, 0, res);
  return res;
}

static struct jfnode *
find_closure(struct jfnode *start, struct jfnode *pool)
{
  struct jfnode *current = start, *next = NULL, *output = NULL;
  while (current)
    {
      struct jfnode *temp;
      for (temp = pool; temp; temp = temp->next)
	{
	  struct jfnode *cur_iter;
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
		  next = create_jfnode(temp->jf->from_name,
		      temp->jf->from_arg,
		      temp->jf->to_name,
		      temp->jf->to_arg,
		      next);
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
	  struct jfnode *last = current;
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
  struct jfnode *base_dlsym, *root, *output;
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

  for (struct jfnode *iter = output; iter; iter = iter->next)
    fprintf(outf, "%s %d\n", iter->jf->from_name, iter->jf->from_arg);

  if (argc >= 4)
    fclose(outf);

  // Free memory
  for (struct jfnode *iter = root, *tmp; iter; iter = tmp) {
    tmp = iter->next;
    free(iter->jf);
    free(iter);
  }

  for (struct jfnode *iter = output, *tmp; iter; iter = tmp) {
    tmp = iter->next;
    free(iter->jf);
    free(iter);
  }

  return 0;
}

