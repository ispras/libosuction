#ifndef DLSYM_DUMP_H
#define DLSYM_DUMP_H 1


void
print_dynamic_symbol_calls (function* func,
			    hash_map<const char*, 
				     hash_set<const char*, 
					      nofree_string_hash>*> *symbols);
#endif
