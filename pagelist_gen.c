#include "types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// writes a filename with num_lines of: <00-(MAX_PAGES-1) page> <R/W operation>
void write_pagelist(const char *filename, int num_lines, int seq_percentage) {
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  int last_page = -1;

  for (int i = 0; i < num_lines; i++) {
    int page;

    if (last_page != -1 && (rand() % 100) < seq_percentage) {
      // sequential
      page = (last_page + 1) % PROC_MAX_PAGES;
    } else {
      // random
      page = rand() % PROC_MAX_PAGES;
    }

    last_page = page;

    char operation = (rand() % 2) ? 'R' : 'W';
    fprintf(file, "%02d %c\n", page, operation);
  }

  printf("Generated %s with %d IO operations, %d%% sequential\n", filename,
         num_lines, seq_percentage);

  fclose(file);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <num_lines> <sequential_percentage>\n", argv[0]);
    exit(2);
  }

  int num_lines = atoi(argv[1]);
  int seq_percentage = atoi(argv[2]);
  assert(num_lines > 0);
  assert(seq_percentage >= 0 && seq_percentage <= 100);

  srand(time(NULL));

  write_pagelist(PAGELIST_P1_FILE, num_lines, seq_percentage);
  write_pagelist(PAGELIST_P2_FILE, num_lines, seq_percentage);
  write_pagelist(PAGELIST_P3_FILE, num_lines, seq_percentage);
  write_pagelist(PAGELIST_P4_FILE, num_lines, seq_percentage);

  printf("Finished\n");

  return 0;
}
