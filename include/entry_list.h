#ifndef ENTRY_LIST_H
#define ENTRY_LIST_H
#include "entry.h"
#include "stdint.h"
#include <sys/types.h>
Entry *init_empty_entry_list(Entry *head_value);

Entry *init_entry_list(int size, Entry *initial_value);
Entry *insert_entry(Entry *head, int index, Entry *new_entry);
Entry *push_entry(Entry *head, Entry *new_entry);
int entry_list_size(Entry *head);
int delete_entry_at_index(Entry *head, int index);
int delete_entries_by_key(Entry *head, uint64_t key);
Entry *find_entry_at_index(Entry *head, int index);
Entry *find_closest_predecessor_entry(Entry *head, uint64_t id);
Entry *find_entry_by_key(Entry *head, uint64_t key);
void wipe_entry_list(Entry *head);
#endif
