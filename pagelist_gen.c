#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_PAGES 32

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

  srand(time(NULL));

  write_pagelist("pagelist_P1.txt", num_lines);
  write_pagelist("pagelist_P2.txt", num_lines);
  write_pagelist("pagelist_P3.txt", num_lines);
  write_pagelist("pagelist_P4.txt", num_lines);

  printf("Finished\n");

  return 0;
}
