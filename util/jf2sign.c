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

struct list_node *output;
struct list_node *current;
struct list_node *next;

static bool
has_duplicates (struct jfunction *jf, struct list_node *list)
{
  struct list_node *iter;
  for (iter = list; iter; iter = iter->next)
    if (!strcmp (jf->from_name, iter->jf->from_name)
	&& jf->from_arg == iter->jf->from_arg)
      return true;
  return false;
}

int main (int argc, char *argv[])
{
  char* from_name;
  int from_arg;
  char* to_name;
  int to_arg;
  struct list_node *root = NULL, *last = NULL;

  if (argc < 3)
    {
      fprintf (stderr, "Two required arguments should be given:\n"
	       " 1) a file with jump functions\n"
	       " 2) a file with base signatures\n"
	       " 3) [Optional] a file to output resulting signatures.\n");
      return 1;
    }

  // Read jump functions
  FILE *f = fopen (argv[1], "r");

  while (f && fscanf (f, "%ms %d %ms %d", &from_name, &from_arg, &to_name, &to_arg) != EOF)
    {
      struct jfunction *temp = (struct jfunction *) malloc (sizeof (struct jfunction));
      temp->from_name = from_name;
      temp->from_arg = from_arg;
      temp->to_name = to_name;
      temp->to_arg = to_arg;

      root = (struct list_node *) malloc (sizeof (struct list_node));
      root->jf = temp;
      root->next = last;

      last = root;
    }

  fclose (f);

  // Read initial calls
  f = fopen (argv[2], "r");

  while (f && fscanf (f, "%ms %d", &from_name, &from_arg) != EOF)
    {
      struct list_node *temp = (struct list_node *) malloc (sizeof (struct list_node));
      temp->jf = (struct jfunction *) malloc (sizeof (struct jfunction));
      temp->jf->from_name = from_name;
      temp->jf->from_arg = from_arg;
      temp->next = current;
      current = temp;
    }

  fclose (f);

  // Match jump function chains
  while (current)
    {
      struct list_node *temp;
      for (temp = root; temp; temp = temp->next)
	{
	  struct list_node *cur_iter;
	  for (cur_iter = current; cur_iter; cur_iter = cur_iter->next)
	    {
	      if (!strcmp (temp->jf->to_name, cur_iter->jf->from_name)
		  && temp->jf->to_arg == cur_iter->jf->from_arg)
		{
		  /* Check duplicates in three disjoint lists */
		  if (has_duplicates (temp->jf, next) ||
		      has_duplicates (temp->jf, output) ||
		      has_duplicates (temp->jf, current))
		    break;

		  // Include jf to dlsym signatures
		  struct list_node *nw = (struct list_node *) malloc (sizeof (struct list_node));
		  nw->jf = temp->jf;
		  nw->next = next;
		  next = nw;
		  break;
		}
	    }
	}

      /* Signatures from CURRENT move to the end of OUTPUT;
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

  // Output results
  FILE *outf = argc < 4 ? stdout : fopen (argv[3], "w");

  last = output;
  while (last)
    {
      fprintf (outf, "%s %d\n", last->jf->from_name, last->jf->from_arg);
      last = last->next;
    }

  if (argc >= 4)
    fclose (outf);

  // Free memory
  while (root)
    {
      last = root;
      root = root->next;
      free (last->jf);
      free (last);
    }
  while (output)
    {
      last = output;
      output = output->next;
      free (last);
    }

  return 0;
}

