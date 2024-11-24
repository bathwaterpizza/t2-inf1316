#include "types.h"
#include "util.h"
#include <assert.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

// working set window parameter
static int k_param;
// page frames available in main memory.
// false = available,
// true = occupied
static bool main_memory[RAM_MAX_PAGES] = {false};
// process page tables
static page_table_entry_t page_table_P1[PROC_MAX_PAGES];
static page_table_entry_t page_table_P2[PROC_MAX_PAGES];
static page_table_entry_t page_table_P3[PROC_MAX_PAGES];
static page_table_entry_t page_table_P4[PROC_MAX_PAGES];

// clears the reference bits in each process' page table
static inline void clear_ref_bits(void) {
  for (int index = 0; index < PROC_MAX_PAGES; index++) {
    page_table_P1[index].flags &= ~PAGE_REFERENCED_BIT;
    page_table_P2[index].flags &= ~PAGE_REFERENCED_BIT;
    page_table_P3[index].flags &= ~PAGE_REFERENCED_BIT;
    page_table_P4[index].flags &= ~PAGE_REFERENCED_BIT;
  }
}

// initialize values for the process' page tables
static inline void init_page_tables(void) {
  for (int index = 0; index < PROC_MAX_PAGES; index++) {
    page_table_P1[index].page_id = index;
    page_table_P2[index].page_id = index;
    page_table_P3[index].page_id = index;
    page_table_P4[index].page_id = index;

    page_table_P1[index].flags = 0;
    page_table_P2[index].flags = 0;
    page_table_P3[index].flags = 0;
    page_table_P4[index].flags = 0;
  }
}

// handle page fault according to Not Recently Used
static void page_algo_NRU(const vmem_io_request_t req) {
  // todo
}

int main(int argc, char **argv) {
  dmsg("vmem_sim started");

  // parse command line args
  if (!(argc == 3 || argc == 4)) {
    fprintf(stderr, "Usage: ./vmem_sim <num rounds> <page algo> [<k param>]\n");
    exit(3);
  }

  // selected page replacement algorithm
  page_algo_t algorithm;
  // function pointer to the selected page replacement algorithm
  page_algo_func_t page_algo_func;

  // one round represents one memory io request from each process,
  // so four requests total
  const int num_rounds = atoi(argv[1]);
  assert(num_rounds > 0);

  if (argc == 4) {
    // set k parameter for working set
    k_param = atoi(argv[3]);
    assert(k_param > 0);
    assert(k_param <= RAM_MAX_PAGES);
  }

  // parse selected paging algorithm
  if (strcasecmp(argv[2], "nru") == 0) {
    algorithm = ALGO_NRU;
    page_algo_func = page_algo_NRU;
  } else if (strcasecmp(argv[2], "2ndc") == 0) {
    algorithm = ALGO_2ndC;

    // TODO: set function pointer
  } else if (strcasecmp(argv[2], "lru") == 0) {
    algorithm = ALGO_LRU;

    // TODO: set function pointer
  } else if (strcasecmp(argv[2], "ws") == 0) {
    algorithm = ALGO_WS;

    // TODO: set function pointer
  } else {
    fprintf(stderr, "Available algorithms: NRU, 2ndC, LRU, WS\n");
    exit(4);
  }

  // open a pipe for each process, to receive memory io requests
  int pipe_P1[2], pipe_P2[2], pipe_P3[2], pipe_P4[2];

  if (pipe(pipe_P1) == -1) {
    perror("Pipe error");
    exit(1);
  }
  if (pipe(pipe_P2) == -1) {
    perror("Pipe error");
    exit(1);
  }
  if (pipe(pipe_P3) == -1) {
    perror("Pipe error");
    exit(1);
  }
  if (pipe(pipe_P4) == -1) {
    perror("Pipe error");
    exit(1);
  }

  // create a semaphore for each process, to sync requests from processes
  sem_t *sem_P1, *sem_P2, *sem_P3, *sem_P4;

  // remove previous semaphores if they exist
  sem_unlink(SEM_P1_NAME);
  sem_unlink(SEM_P2_NAME);
  sem_unlink(SEM_P3_NAME);
  sem_unlink(SEM_P4_NAME);

  sem_P1 = sem_open(SEM_P1_NAME, O_CREAT, 0666, 0);
  if (sem_P1 == SEM_FAILED) {
    perror("Sem error");
    exit(2);
  }
  sem_P2 = sem_open(SEM_P2_NAME, O_CREAT, 0666, 0);
  if (sem_P2 == SEM_FAILED) {
    perror("Sem error");
    exit(2);
  }
  sem_P3 = sem_open(SEM_P3_NAME, O_CREAT, 0666, 0);
  if (sem_P3 == SEM_FAILED) {
    perror("Sem error");
    exit(2);
  }
  sem_P4 = sem_open(SEM_P4_NAME, O_CREAT, 0666, 0);
  if (sem_P4 == SEM_FAILED) {
    perror("Sem error");
    exit(2);
  }

  // spawn processes (P1, P2, P3, P4) simulator
  pid_t procs_pid = fork();
  if (procs_pid < 0) {
    perror("Fork error");
    exit(5);
  } else if (procs_pid == 0) {
    // child

    // using environment variables to pass pipe fds and num rounds
    char pipe_P1_read_str[12], pipe_P1_write_str[12], pipe_P2_read_str[12],
        pipe_P2_write_str[12], pipe_P3_read_str[12], pipe_P3_write_str[12],
        pipe_P4_read_str[12], pipe_P4_write_str[12], num_rounds_str[12];
    sprintf(pipe_P1_read_str, "%d", pipe_P1[PIPE_READ]);
    sprintf(pipe_P1_write_str, "%d", pipe_P1[PIPE_WRITE]);
    sprintf(pipe_P2_read_str, "%d", pipe_P2[PIPE_READ]);
    sprintf(pipe_P2_write_str, "%d", pipe_P2[PIPE_WRITE]);
    sprintf(pipe_P3_read_str, "%d", pipe_P3[PIPE_READ]);
    sprintf(pipe_P3_write_str, "%d", pipe_P3[PIPE_WRITE]);
    sprintf(pipe_P4_read_str, "%d", pipe_P4[PIPE_READ]);
    sprintf(pipe_P4_write_str, "%d", pipe_P4[PIPE_WRITE]);
    sprintf(num_rounds_str, "%d", num_rounds);

    setenv("PIPE_P1_READ", pipe_P1_read_str, 1);
    setenv("PIPE_P1_WRITE", pipe_P1_write_str, 1);
    setenv("PIPE_P2_READ", pipe_P2_read_str, 1);
    setenv("PIPE_P2_WRITE", pipe_P2_write_str, 1);
    setenv("PIPE_P3_READ", pipe_P3_read_str, 1);
    setenv("PIPE_P3_WRITE", pipe_P3_write_str, 1);
    setenv("PIPE_P4_READ", pipe_P4_read_str, 1);
    setenv("PIPE_P4_WRITE", pipe_P4_write_str, 1);
    setenv("NUM_ROUNDS", num_rounds_str, 1);

    execl("./procs_sim", "procs_sim", NULL);
  }

  // close pipe writes, as we will only be reading here
  close(pipe_P1[PIPE_WRITE]);
  close(pipe_P2[PIPE_WRITE]);
  close(pipe_P3[PIPE_WRITE]);
  close(pipe_P4[PIPE_WRITE]);

  init_page_tables();

  // main loop, post sem and read memory io requests from processes' pipes.
  // each iteration is a round, meaning one IO request from each process
  for (int i = 1; i <= num_rounds; i++) {
    vmem_io_request_t req;

    // Process 1
    sem_post(sem_P1);

    if (read(pipe_P1[PIPE_READ], &req, sizeof(req)) == -1) {
      perror("Pipe read error");
      exit(9);
    }
    assert(req.proc_page_id >= 0 && req.proc_page_id < PROC_MAX_PAGES);
    assert(req.operation == 'R' || req.operation == 'W');

    dmsg("vmem_sim got P1: %02d %c", req.proc_page_id, req.operation);

    // code

    // Process 2
    sem_post(sem_P2);

    if (read(pipe_P2[PIPE_READ], &req, sizeof(req)) == -1) {
      perror("Pipe read error");
      exit(9);
    }
    assert(req.proc_page_id >= 0 && req.proc_page_id < PROC_MAX_PAGES);
    assert(req.operation == 'R' || req.operation == 'W');

    dmsg("vmem_sim got P2: %02d %c", req.proc_page_id, req.operation);

    // code

    // Process 3
    sem_post(sem_P3);

    if (read(pipe_P3[PIPE_READ], &req, sizeof(req)) == -1) {
      perror("Pipe read error");
      exit(9);
    }
    assert(req.proc_page_id >= 0 && req.proc_page_id < PROC_MAX_PAGES);
    assert(req.operation == 'R' || req.operation == 'W');

    dmsg("vmem_sim got P3: %02d %c", req.proc_page_id, req.operation);

    // code

    // Process 1
    sem_post(sem_P4);

    if (read(pipe_P4[PIPE_READ], &req, sizeof(req)) == -1) {
      perror("Pipe read error");
      exit(9);
    }
    assert(req.proc_page_id >= 0 && req.proc_page_id < PROC_MAX_PAGES);
    assert(req.operation == 'R' || req.operation == 'W');

    dmsg("vmem_sim got P4: %02d %c", req.proc_page_id, req.operation);

    // code

    if (i % REF_BITS_CLEAR_INTERVAL == 0)
      clear_ref_bits();

    dmsg("vmem_sim finished round %d", i);
  }

  // TODO: print results

  // cleanup
  close(pipe_P1[PIPE_READ]);
  close(pipe_P2[PIPE_READ]);
  close(pipe_P3[PIPE_READ]);
  close(pipe_P4[PIPE_READ]);
  sem_close(sem_P1);
  sem_close(sem_P2);
  sem_close(sem_P3);
  sem_close(sem_P4);
  sem_unlink(SEM_P1_NAME);
  sem_unlink(SEM_P2_NAME);
  sem_unlink(SEM_P3_NAME);
  sem_unlink(SEM_P4_NAME);
  unsetenv("PIPE_P1_READ");
  unsetenv("PIPE_P1_WRITE");
  unsetenv("PIPE_P2_READ");
  unsetenv("PIPE_P2_WRITE");
  unsetenv("PIPE_P3_READ");
  unsetenv("PIPE_P3_WRITE");
  unsetenv("PIPE_P4_READ");
  unsetenv("PIPE_P4_WRITE");
  unsetenv("NUM_ROUNDS");

  dmsg("vmem_sim finished");

  return 0;
}
