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
// amount of pages each process' virtual memory has
#define PROC_MAX_PAGES 32
// amount of pages the simulated physical memory has
#define RAM_MAX_PAGES 16

// how often should page entries' referenced bit be cleared, in rounds
#define REF_BITS_CLEAR_INTERVAL 3
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

// page flags
typedef unsigned char page_flags_t;

// process page table entry
// NOTE: each value must be initialized in init_page_tables()
typedef struct {
  // page table data
  int page_id;        // 0-31 page ID
  page_flags_t flags; // page flags
  /*
   * Bit 0b00000001: Valid (page is in main memory)
   * Bit 0b00000010: Referenced (page has been read/written recently)
   * Bit 0b00000100: Modified (page has been written to, "dirty")
   */
  int page_frame; // page index in main memory, -1 if not in memory

  // statistics
  int read_count;           // amount of R requests to this page
  int write_count;          // amount of W requests to this page
  int page_fault_count;     // amount of total page faults to this page
  int modified_fault_count; // amount of dirty page faults to this page
} page_table_entry_t;
