#include "types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// writes a filename with num_lines of: <00-(MAX_PAGES-1) page> <R/W operation>
void write_pagelist(const char *filename, int num_lines,
                    int locality_percentage) {
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  int last_page = -1;

  for (int i = 0; i < num_lines; i++) {
    int page;

    if (last_page != -1 && (rand() % 100) < locality_percentage) {
      // local
      int locality_choice = rand() % 3;
      if (locality_choice == 0) {
        // same page
        page = last_page;
      } else if (locality_choice == 1) {
        // next page
        page = (last_page + 1) % PROC_MAX_PAGES;
      } else {
        // previous page
        page = (last_page - 1 + PROC_MAX_PAGES) % PROC_MAX_PAGES;
      }
    } else {
      // random
      page = rand() % PROC_MAX_PAGES;
    }

    last_page = page;

    char operation = (rand() % 2) ? 'R' : 'W';
    fprintf(file, "%02d %c\n", page, operation);
  }

  printf("Generated %s with %d IO operations, %d%% locality\n", filename,
         num_lines, locality_percentage);

  fclose(file);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <num_lines> <locality_percentage>\n", argv[0]);
    exit(2);
  }

  int num_lines = atoi(argv[1]);
  int locality_percentage = atoi(argv[2]);
  assert(num_lines > 0);
  assert(locality_percentage >= 0 && locality_percentage <= 100);

  srand(time(NULL));

  write_pagelist(PAGELIST_P1_FILE, num_lines, locality_percentage);
  write_pagelist(PAGELIST_P2_FILE, num_lines, locality_percentage);
  write_pagelist(PAGELIST_P3_FILE, num_lines, locality_percentage);
  write_pagelist(PAGELIST_P4_FILE, num_lines, locality_percentage);

  printf("Finished\n");

  return 0;
}
