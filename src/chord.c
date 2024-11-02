#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/hash.h"
#include "chord.h"
#include "chord_arg_parser.h"

void printKey(uint64_t key) { printf("%" PRIu64, key); }

int main(int argc, char *argv[]) {
  struct chord_arguments args = chord_parseopt(argc, argv);

  return 0;
}
