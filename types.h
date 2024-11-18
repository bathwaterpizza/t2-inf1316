#pragma once

// enable debug output
#define DEBUG

// convenient pipe fd array indexers
#define PIPE_READ 0
#define PIPE_WRITE 1

// semaphores to sync
#define SEM_P1_NAME "/sem_P1"
#define SEM_P2_NAME "/sem_P2"
#define SEM_P3_NAME "/sem_P3"
#define SEM_P4_NAME "/sem_P4"

// files where the memory io requests for each process will be stored
#define PAGELIST_P1_FILE "pagelist_P1.txt"
#define PAGELIST_P2_FILE "pagelist_P2.txt"
#define PAGELIST_P3_FILE "pagelist_P3.txt"
#define PAGELIST_P4_FILE "pagelist_P4.txt"
// amount of pages each process' memory has
#define PROC_MAX_PAGES 32

// used to identify the algorithm being used in this execution
typedef enum {
  ALGO_NRU,  // Not Recently Used
  ALGO_2ndC, // Second Chance
  ALGO_LRU,  // Least Recently Used
  ALGO_WS    // Working Set (takes k param)
} page_algo_t;

// data being sent from procs_sim to vmem_sim through processes' pipes
typedef struct {
  int proc_page_id; // 0-31 page id within the process' memory
  char operation;   // 'R' or 'W' for read or write
} vmem_io_request_t;
