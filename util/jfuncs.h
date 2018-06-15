#ifndef JFUNCS_H
#define JFUNCS_H

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

struct jfnode *create_jfnode(const char *from_name, int from_arg,
			     const char *to_name, int to_arg,
			     struct jfnode *next);
void read_jf(FILE *f, struct jfnode **list);
struct jfnode *read_base(FILE *f);
struct jfnode *find_closure(struct jfnode *start, struct jfnode *pool);
void print_jflist(FILE *f, struct jfnode *list);
void free_jflist(struct jfnode **list);

#endif /* jfuncs.h */
