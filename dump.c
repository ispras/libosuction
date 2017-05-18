#include <iostream>

#include "dump.h"
#include "hash-map.h"
#include "hash-set.h"
#include "hash-traits.h"

void
print_dynamic_symbol_calls (function* func,
			    hash_map<const char*, 
				     hash_set<const char*, 
					      nofree_string_hash>*> *symbols)
{
  fprintf (dump_file, "Function->callee->[symbols]:\n");
  for (auto it = dynamic_symbols.->begin (); 
       it != dynamic_symbols->end ();
       ++it)
    {
      fprintf(dump_file, "%s->%s->[", function_name(func),
	      (*it).first);
      for (auto it2 = (*it).second->begin (); 
	   it2 != (*it).second->end ();
	   ++it2)
	{
	  if (it2 != (*it).second->begin ())
	    fprintf (dump_file, ",");
	  fprintf (dump_file, "%s", *it2); 
	}
      fprintf(dump_file, "]\n");
    }
  fprintf(dump_file, "\n");
}

