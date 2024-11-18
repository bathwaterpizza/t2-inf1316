#include "types.h"
#include "util.h"
#include <assert.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
  dmsg("procs_sim started");
  int pipe_P1[2], pipe_P2[2], pipe_P3[2], pipe_P4[2];

  // retrieve args from environment variables
  const char *pipe_P1_read_str = getenv("PIPE_P1_READ");
  const char *pipe_P1_write_str = getenv("PIPE_P1_WRITE");
  const char *pipe_P2_read_str = getenv("PIPE_P2_READ");
  const char *pipe_P2_write_str = getenv("PIPE_P2_WRITE");
  const char *pipe_P3_read_str = getenv("PIPE_P3_READ");
  const char *pipe_P3_write_str = getenv("PIPE_P3_WRITE");
  const char *pipe_P4_read_str = getenv("PIPE_P4_READ");
  const char *pipe_P4_write_str = getenv("PIPE_P4_WRITE");
  const char *num_rounds_str = getenv("NUM_ROUNDS");

  const int num_rounds = atoi(num_rounds_str);
  pipe_P1[PIPE_READ] = atoi(pipe_P1_read_str);
  pipe_P1[PIPE_WRITE] = atoi(pipe_P1_write_str);
  pipe_P2[PIPE_READ] = atoi(pipe_P2_read_str);
  pipe_P2[PIPE_WRITE] = atoi(pipe_P2_write_str);
  pipe_P3[PIPE_READ] = atoi(pipe_P3_read_str);
  pipe_P3[PIPE_WRITE] = atoi(pipe_P3_write_str);
  pipe_P4[PIPE_READ] = atoi(pipe_P4_read_str);
  pipe_P4[PIPE_WRITE] = atoi(pipe_P4_write_str);
  assert(num_rounds > 0);

  // close pipe reads, as we will only be writing here
  close(pipe_P1[PIPE_READ]);
  close(pipe_P2[PIPE_READ]);
  close(pipe_P3[PIPE_READ]);
  close(pipe_P4[PIPE_READ]);

  // open syncing semaphores
  sem_t *sem_P1, *sem_P2, *sem_P3, *sem_P4;

  sem_P1 = sem_open(SEM_P1_NAME, 0);
  if (sem_P1 == SEM_FAILED) {
    perror("Sem error");
    exit(2);
  }
  sem_P2 = sem_open(SEM_P2_NAME, 0);
  if (sem_P2 == SEM_FAILED) {
    perror("Sem error");
    exit(2);
  }
  sem_P3 = sem_open(SEM_P3_NAME, 0);
  if (sem_P3 == SEM_FAILED) {
    perror("Sem error");
    exit(2);
  }
  sem_P4 = sem_open(SEM_P4_NAME, 0);
  if (sem_P4 == SEM_FAILED) {
    perror("Sem error");
    exit(2);
  }

  // open pagelist files containing the memory io requests
  FILE *pagelist_P1 = fopen(PAGELIST_P1_FILE, "r");
  if (pagelist_P1 == NULL) {
    perror("File error");
    exit(6);
  }
  FILE *pagelist_P2 = fopen(PAGELIST_P2_FILE, "r");
  if (pagelist_P2 == NULL) {
    perror("File error");
    exit(6);
  }
  FILE *pagelist_P3 = fopen(PAGELIST_P3_FILE, "r");
  if (pagelist_P3 == NULL) {
    perror("File error");
    exit(6);
  }
  FILE *pagelist_P4 = fopen(PAGELIST_P4_FILE, "r");
  if (pagelist_P4 == NULL) {
    perror("File error");
    exit(6);
  }

  // main loop, wait for sem and send memory io request through pipes
  for (int i = 0; i < num_rounds; i++) {
    vmem_io_request_t req;

    // Process 1
    sem_wait(sem_P1);

    if (fscanf(pagelist_P1, "%d %c", &req.proc_page_id, &req.operation) != 2) {
      fprintf(stderr, "Error reading pagelist_P1\n");
      exit(7);
    }
    if (write(pipe_P1[PIPE_WRITE], &req, sizeof(req)) == -1) {
      perror("Pipe write error");
      exit(8);
    }

    dmsg("procs_sim sent P1");

    // Process 2
    sem_wait(sem_P2);

    if (fscanf(pagelist_P2, "%d %c", &req.proc_page_id, &req.operation) != 2) {
      fprintf(stderr, "Error reading pagelist_P2\n");
      exit(7);
    }
    if (write(pipe_P2[PIPE_WRITE], &req, sizeof(req)) == -1) {
      perror("Pipe write error");
      exit(8);
    }
    dmsg("procs_sim sent P2");

    // Process 3
    sem_wait(sem_P3);

    if (fscanf(pagelist_P3, "%d %c", &req.proc_page_id, &req.operation) != 2) {
      fprintf(stderr, "Error reading pagelist_P3\n");
      exit(7);
    }
    if (write(pipe_P3[PIPE_WRITE], &req, sizeof(req)) == -1) {
      perror("Pipe write error");
      exit(8);
    }

    dmsg("procs_sim sent P3");

    // Process 4
    sem_wait(sem_P4);

    if (fscanf(pagelist_P4, "%d %c", &req.proc_page_id, &req.operation) != 2) {
      fprintf(stderr, "Error reading pagelist_P4\n");
      exit(7);
    }
    if (write(pipe_P4[PIPE_WRITE], &req, sizeof(req)) == -1) {
      perror("Pipe write error");
      exit(8);
    }

    dmsg("procs_sim sent P4");

    dmsg("procs_sim finished round %d", i);
  }

  // cleanup
  sem_close(sem_P1);
  sem_close(sem_P2);
  sem_close(sem_P3);
  sem_close(sem_P4);
  close(pipe_P1[PIPE_WRITE]);
  close(pipe_P2[PIPE_WRITE]);
  close(pipe_P3[PIPE_WRITE]);
  close(pipe_P4[PIPE_WRITE]);
  fclose(pagelist_P1);
  fclose(pagelist_P2);
  fclose(pagelist_P3);
  fclose(pagelist_P4);

  dmsg("procs_sim finished");

  return 0;
}
