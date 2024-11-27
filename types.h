/*
 * Exit code table
 * 00 - ok
 * 01 - pipe create error
 * 02 - sem create error
 * 03 - invalid arg count
 * 04 - invalid algorithm arg
 * 05 - fork error
 * 06 - file error
 * 07 - pagelist read error
 * 08 - pipe write error
 * 09 - pipe read error
 * 10 - invalid process ID
 */

#pragma once

#include <stdint.h>

// enable debug output
// #define DEBUG

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
// amount of pages each process' virtual memory has
#define PROC_MAX_PAGES 32
// amount of page frames the simulated physical memory has
#define RAM_MAX_PAGES 16

// how often should the R bits be cleared, as well as aging shifted, in rounds
#define REF_CLEAR_INTERVAL 4
// page flags bits
#define PAGE_VALID_BIT 0b00000001
#define PAGE_REFERENCED_BIT 0b00000010
#define PAGE_MODIFIED_BIT 0b00000100

// used to identify the algorithm being used in this execution
typedef enum {
  ALGO_NRU,  // Not Recently Used
  ALGO_2ndC, // Second Chance
  ALGO_LRU,  // Least Recently Used/Aging
  ALGO_WS    // Working Set (takes k param)
} page_algo_t;
extern const char *PAGE_ALGO_STR[];

// data being sent from procs_sim to vmem_sim through processes' pipes
typedef struct {
  int proc_id;      // 1-4 process ID
  int proc_page_id; // 0-31 page ID within the process' memory
  char operation;   // 'R' or 'W' for read or write
} vmem_io_request_t;

// function pointer type for page replacement algorithms
typedef void (*page_algo_func_t)(const vmem_io_request_t);

// page flags bits
typedef unsigned char page_flags_t;
// page age bits for LRU
typedef uint32_t page_age_bits_t;

// process page table entry
// NOTE: each value must be initialized in init_page_data()
typedef struct {
  // page table data
  int page_id;        // 0-31 page ID
  page_flags_t flags; // page flags
  /*
   * Bit 0b00000001: Valid      (page is in main memory)
   * Bit 0b00000010: Referenced (page has been accessed recently)
   * Bit 0b00000100: Modified   (page has been written to, "dirty")
   */
  page_age_bits_t age_bits; // page age bit vector for LRU
  int page_frame;           // page index in main memory, -1 if not in memory

  // statistics
  int read_count;           // amount of R requests to this page
  int write_count;          // amount of W requests to this page
  int page_fault_count;     // amount of total page faults caused by this page
  int modified_fault_count; // amount of dirty page faults caused by this page
} page_table_entry_t;

// queue node
typedef struct node {
  int data;
  struct node *next;
} node_t;

// queue
typedef struct {
  node_t *front;
  node_t *rear;
} queue_t;
