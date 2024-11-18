#include "types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_PAGES 32

// writes a filename with num_lines of: <00-(MAX_PAGES-1) page> <R/W operation>
void write_pagelist(const char *filename, int num_lines) {
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  for (int i = 0; i < num_lines; i++) {
    int page = rand() % MAX_PAGES;
    char operation = (rand() % 2) ? 'R' : 'W';
    fprintf(file, "%02d %c\n", page, operation);
  }

  printf("Generated %s with %d IO operations\n", filename, num_lines);

  fclose(file);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Invalid args\n");
    exit(2);
  }

  int num_lines = atoi(argv[1]);
  assert(num_lines > 0);

  srand(time(NULL));

  write_pagelist(PAGELIST_P1_FILE, num_lines);
  write_pagelist(PAGELIST_P2_FILE, num_lines);
  write_pagelist(PAGELIST_P3_FILE, num_lines);
  write_pagelist(PAGELIST_P4_FILE, num_lines);

  printf("Finished\n");

  return 0;
}
