#include <stdlib.h>
#include <string.h>

#include "../include/entry.h"
#include "../include/entry_list.h"
// Initializes an "empty" list (list with only head).
// head_value will be the value of the head.
Entry *init_empty_entry_list(Entry *head_value) {
  Entry *head;
  InitDQ(head, Entry);
  memcpy(head, head_value, sizeof(Entry));
  head->next = head->prev = head;
  return head;
}

// Initializes a list of entries with given size and sets all entries (except
// head) to the given initial value. If size is 0, only the head is initialized
//(values of head != initial value, do not use head values (other than next and
// prev) for anything). If initial_value is NULL, treats size as 0, and
// vice-versa.
Entry *init_entry_list(int size, Entry *initial_value) {
  Entry *head = init_empty_entry_list(initial_value);
  if (size == 0 || initial_value == NULL) {
    return head;
  }

  Entry *curr;
  Entry *prev = head;
  for (int i = 0; i < size; i++) {
    curr = malloc(sizeof(Entry));
    memcpy(curr, initial_value, sizeof(Entry));
    prev->next = curr;
    curr->prev = prev;
    curr->next = head;
    head->prev = curr;
    prev = curr;
  }
  return head;
}

// Insert copy of new entry to given entry list at given index.
// Returns a deep copy of new entry (next and prev have been changed)
Entry *insert_entry(Entry *head, int index, Entry *new_entry) {
  if (index == entry_list_size(head)) {
    return push_entry(head, new_entry);
  }
  if (index > entry_list_size(head)) {
    return NULL;
  }

  Entry *curr = head;
  for (int i = 0; i < index; i++) {
    curr = curr->next;
  }
  Entry *new_entry_copy = malloc(sizeof(Entry));
  memcpy(new_entry_copy, new_entry, sizeof(Entry));
  new_entry_copy->next = curr->next;
  new_entry_copy->prev = curr;
  curr->next->prev = new_entry_copy;
  curr->next = new_entry_copy;
  return new_entry_copy;
}

// Inserts the new_entry to the end of the given entry list.
// Note that a copy of the new entry is pushed to the entry list.
// Returns a deep copy of the new entry (next and prev have been changed).
Entry *push_entry(Entry *head, Entry *new_entry) {
  Entry *curr = head->next;
  while (curr->next != head) {
    curr = curr->next;
  }

  Entry *new_entry_copy = malloc(sizeof(Entry));
  memcpy(new_entry_copy, new_entry, sizeof(Entry));
  new_entry_copy->next = curr->next;
  new_entry_copy->prev = curr;
  curr->next->prev = new_entry_copy;
  curr->next = new_entry_copy;

  return new_entry_copy;
}

int entry_list_size(Entry *head) {
  Entry *curr = head->next;
  int count = 0;
  while (curr != head) {
    count++;
    curr = curr->next;
  }
  return count;
}

// Deletes the entry in the given entry list at given index.
// Returns 1 if deleted (index < size), 0 if not.
int delete_entry_at_index(Entry *head, int index) {
  if (index >= entry_list_size(head)) {
    return 0;
  }
  Entry *curr = head->next;
  for (int i = 0; i < index; i++) {
    curr = curr->next;
  }
  curr->prev->next = curr->next;
  curr->next->prev = curr->prev;
  free(curr);

  return 1;
}
void wipe_entry_list(Entry *head) {

  Entry *curr = head->next;

  while (curr != head) {
    Entry *tmp = curr;
    curr = curr->next;
    free(tmp);
  }

  head->next = head->prev = head;
}
// Finds entry at given index of entry list
// returns the entry at the given index
// Note that if index >= size, it will return last entry
Entry *find_entry_at_index(Entry *head, int index) {
  Entry *curr = head->next;
  int curr_index = 0;
  while (curr_index < index && curr->next != head) {
    curr = curr->next;
    curr_index++;
  }
  return curr;
}

Entry *find_entry_by_key(Entry *head, int key) {
  Entry *curr = head->next;
  while (curr != head) {
    if (curr->key == key) {
      return curr;
    }
    curr = curr->next;
  }
  // We weren't looking for head and didn't find a match
  if (head->key != key && curr->key == key) {
    return NULL;
  }
  return curr;
}

Entry *find_closest_predecessor_entry(Entry *head, uint64_t id) {
  if (entry_list_size(head) == 0) {
    return NULL;
  }
  Entry *closest_predecessor = head->next;
  Entry *curr = head->next;
  while (curr->next != head) {
    if (is_between_exclusive(closest_predecessor->key, curr->key, id)) {
      closest_predecessor = curr;
    }
    curr = curr->next;
  }
  return closest_predecessor;
}
