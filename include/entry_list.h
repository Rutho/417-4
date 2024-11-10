#include "entry.h"
Entry *init_empty_entry_list(Entry *head_value);

Entry *init_entry_list(int size, Entry *initial_value);
Entry *insert_entry(Entry *head, int index, Entry *new_entry);
Entry *push_entry(Entry *head, Entry *new_entry);
int entry_list_size(Entry *head);
int delete_entry_at_index(Entry *head, int index);
Entry *find_entry_at_index(Entry *head, int index);
Entry *find_closest_predecessor_entry(Entry *head, uint64_t id);
