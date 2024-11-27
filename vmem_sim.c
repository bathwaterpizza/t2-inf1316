#include "types.h"
#include "util.h"
#include <assert.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

// selected page replacement algorithm
static page_algo_t algorithm;
// pointer to the selected page replacement algorithm's function
static page_algo_func_t page_algo_func;
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
// process page queues for Second Chance
static queue_t *page_queue_P1;
static queue_t *page_queue_P2;
static queue_t *page_queue_P3;
static queue_t *page_queue_P4;
// process working sets for Working Set(k)
static set_t *page_wset_P1;
static set_t *page_wset_P2;
static set_t *page_wset_P3;
static set_t *page_wset_P4;
// global clock time for Working Set(k) page age comparison,
// incremented every round
static int clock_counter = 0;

// clears the reference bits in each process' page table
static inline void clear_ref_bits(void) {
  assert(algorithm != ALGO_2ndC);

  for (int i = 0; i < PROC_MAX_PAGES; i++) {
    page_table_P1[i].flags &= ~PAGE_REFERENCED_BIT;
    page_table_P2[i].flags &= ~PAGE_REFERENCED_BIT;
    page_table_P3[i].flags &= ~PAGE_REFERENCED_BIT;
    page_table_P4[i].flags &= ~PAGE_REFERENCED_BIT;
  }
}

// initialize values for the process' page tables and other data structures
static void init_page_data(void) {
  for (int i = 0; i < PROC_MAX_PAGES; i++) {
    page_table_P1[i].page_id = i;
    page_table_P2[i].page_id = i;
    page_table_P3[i].page_id = i;
    page_table_P4[i].page_id = i;

    page_table_P1[i].flags = 0;
    page_table_P2[i].flags = 0;
    page_table_P3[i].flags = 0;
    page_table_P4[i].flags = 0;

    if (algorithm == ALGO_2ndC) {
      page_queue_P1 = create_queue();
      page_queue_P2 = create_queue();
      page_queue_P3 = create_queue();
      page_queue_P4 = create_queue();
    }

    if (algorithm == ALGO_LRU) {
      page_table_P1[i].age_bits = 0;
      page_table_P2[i].age_bits = 0;
      page_table_P3[i].age_bits = 0;
      page_table_P4[i].age_bits = 0;
    }

    if (algorithm == ALGO_WS) {
      page_table_P1[i].age_clock = 0;
      page_table_P2[i].age_clock = 0;
      page_table_P3[i].age_clock = 0;
      page_table_P4[i].age_clock = 0;

      page_wset_P1 = create_set();
      page_wset_P2 = create_set();
      page_wset_P3 = create_set();
      page_wset_P4 = create_set();
    }

    page_table_P1[i].page_frame = -1;
    page_table_P2[i].page_frame = -1;
    page_table_P3[i].page_frame = -1;
    page_table_P4[i].page_frame = -1;

    page_table_P1[i].read_count = 0;
    page_table_P2[i].read_count = 0;
    page_table_P3[i].read_count = 0;
    page_table_P4[i].read_count = 0;

    page_table_P1[i].write_count = 0;
    page_table_P2[i].write_count = 0;
    page_table_P3[i].write_count = 0;
    page_table_P4[i].write_count = 0;

    page_table_P1[i].page_fault_count = 0;
    page_table_P2[i].page_fault_count = 0;
    page_table_P3[i].page_fault_count = 0;
    page_table_P4[i].page_fault_count = 0;

    page_table_P1[i].modified_fault_count = 0;
    page_table_P2[i].modified_fault_count = 0;
    page_table_P3[i].modified_fault_count = 0;
    page_table_P4[i].modified_fault_count = 0;
  }
}

// returns whether the requested page is in memory, by checking the valid bit
static inline bool is_in_memory(const vmem_io_request_t req) {
  switch (req.proc_id) {
  case 1:
    return (bool)(page_table_P1[req.proc_page_id].flags & PAGE_VALID_BIT);
  case 2:
    return (bool)(page_table_P2[req.proc_page_id].flags & PAGE_VALID_BIT);
  case 3:
    return (bool)(page_table_P3[req.proc_page_id].flags & PAGE_VALID_BIT);
  case 4:
    return (bool)(page_table_P4[req.proc_page_id].flags & PAGE_VALID_BIT);
  default:
    fprintf(stderr, "Invalid process ID: %d\n", req.proc_id);
    exit(10);
  }
}

// returns whether there is memory available to store a new page
static inline bool is_memory_available(void) {
  for (int i = 0; i < RAM_MAX_PAGES; i++) {
    if (!main_memory[i])
      return true;
  }

  return false;
}

// increments the read or write count of the requested page
static inline void increment_rw_count(const vmem_io_request_t req) {
  switch (req.proc_id) {
  case 1:
    req.operation == 'R' ? page_table_P1[req.proc_page_id].read_count++
                         : page_table_P1[req.proc_page_id].write_count++;
    break;
  case 2:
    req.operation == 'R' ? page_table_P2[req.proc_page_id].read_count++
                         : page_table_P2[req.proc_page_id].write_count++;
    break;
  case 3:
    req.operation == 'R' ? page_table_P3[req.proc_page_id].read_count++
                         : page_table_P3[req.proc_page_id].write_count++;
    break;
  case 4:
    req.operation == 'R' ? page_table_P4[req.proc_page_id].read_count++
                         : page_table_P4[req.proc_page_id].write_count++;
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", req.proc_id);
    exit(10);
  }
}

// increments the page fault count of the requested page
static inline void increment_fault_count(const vmem_io_request_t req,
                                         const bool is_modified) {
  switch (req.proc_id) {
  case 1:
    page_table_P1[req.proc_page_id].page_fault_count++;

    if (is_modified)
      page_table_P1[req.proc_page_id].modified_fault_count++;
    break;
  case 2:
    page_table_P2[req.proc_page_id].page_fault_count++;

    if (is_modified)
      page_table_P2[req.proc_page_id].modified_fault_count++;
    break;
  case 3:
    page_table_P3[req.proc_page_id].page_fault_count++;

    if (is_modified)
      page_table_P3[req.proc_page_id].modified_fault_count++;
    break;
  case 4:
    page_table_P4[req.proc_page_id].page_fault_count++;

    if (is_modified)
      page_table_P4[req.proc_page_id].modified_fault_count++;
    break;
  default:
    fprintf(stderr, "invalid process id: %d\n", req.proc_id);
    exit(10);
  }
}

// set or clear the modified bit of the requested page
static inline void set_modified(const int proc_id, const int proc_page_id,
                                const bool value) {
  switch (proc_id) {
  case 1:
    if (value)
      page_table_P1[proc_page_id].flags |= PAGE_MODIFIED_BIT;
    else
      page_table_P1[proc_page_id].flags &= ~PAGE_MODIFIED_BIT;
    break;
  case 2:
    if (value)
      page_table_P2[proc_page_id].flags |= PAGE_MODIFIED_BIT;
    else
      page_table_P2[proc_page_id].flags &= ~PAGE_MODIFIED_BIT;
    break;
  case 3:
    if (value)
      page_table_P3[proc_page_id].flags |= PAGE_MODIFIED_BIT;
    else
      page_table_P3[proc_page_id].flags &= ~PAGE_MODIFIED_BIT;
    break;
  case 4:
    if (value)
      page_table_P4[proc_page_id].flags |= PAGE_MODIFIED_BIT;
    else
      page_table_P4[proc_page_id].flags &= ~PAGE_MODIFIED_BIT;
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// get the modified bit of the requested page
static inline bool get_modified(const int proc_id, const int proc_page_id) {
  switch (proc_id) {
  case 1:
    return (bool)(page_table_P1[proc_page_id].flags & PAGE_MODIFIED_BIT);
  case 2:
    return (bool)(page_table_P2[proc_page_id].flags & PAGE_MODIFIED_BIT);
  case 3:
    return (bool)(page_table_P3[proc_page_id].flags & PAGE_MODIFIED_BIT);
  case 4:
    return (bool)(page_table_P4[proc_page_id].flags & PAGE_MODIFIED_BIT);
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// set or clear the referenced bit of the requested page
static inline void set_referenced(const int proc_id, const int proc_page_id,
                                  const bool value) {
  switch (proc_id) {
  case 1:
    if (value)
      page_table_P1[proc_page_id].flags |= PAGE_REFERENCED_BIT;
    else
      page_table_P1[proc_page_id].flags &= ~PAGE_REFERENCED_BIT;
    break;
  case 2:
    if (value)
      page_table_P2[proc_page_id].flags |= PAGE_REFERENCED_BIT;
    else
      page_table_P2[proc_page_id].flags &= ~PAGE_REFERENCED_BIT;
    break;
  case 3:
    if (value)
      page_table_P3[proc_page_id].flags |= PAGE_REFERENCED_BIT;
    else
      page_table_P3[proc_page_id].flags &= ~PAGE_REFERENCED_BIT;
    break;
  case 4:
    if (value)
      page_table_P4[proc_page_id].flags |= PAGE_REFERENCED_BIT;
    else
      page_table_P4[proc_page_id].flags &= ~PAGE_REFERENCED_BIT;
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// get the referenced bit of the requested page
static inline bool get_referenced(const int proc_id, const int proc_page_id) {
  switch (proc_id) {
  case 1:
    return (bool)(page_table_P1[proc_page_id].flags & PAGE_REFERENCED_BIT);
  case 2:
    return (bool)(page_table_P2[proc_page_id].flags & PAGE_REFERENCED_BIT);
  case 3:
    return (bool)(page_table_P3[proc_page_id].flags & PAGE_REFERENCED_BIT);
  case 4:
    return (bool)(page_table_P4[proc_page_id].flags & PAGE_REFERENCED_BIT);
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// set or clear the valid bit of the requested page
static inline void set_valid(const int proc_id, const int proc_page_id,
                             const bool value) {
  switch (proc_id) {
  case 1:
    if (value)
      page_table_P1[proc_page_id].flags |= PAGE_VALID_BIT;
    else
      page_table_P1[proc_page_id].flags &= ~PAGE_VALID_BIT;
    break;
  case 2:
    if (value)
      page_table_P2[proc_page_id].flags |= PAGE_VALID_BIT;
    else
      page_table_P2[proc_page_id].flags &= ~PAGE_VALID_BIT;
    break;
  case 3:
    if (value)
      page_table_P3[proc_page_id].flags |= PAGE_VALID_BIT;
    else
      page_table_P3[proc_page_id].flags &= ~PAGE_VALID_BIT;
    break;
  case 4:
    if (value)
      page_table_P4[proc_page_id].flags |= PAGE_VALID_BIT;
    else
      page_table_P4[proc_page_id].flags &= ~PAGE_VALID_BIT;
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// get the valid bit of the requested page
static inline bool get_valid(const int proc_id, const int proc_page_id) {
  switch (proc_id) {
  case 1:
    return (bool)(page_table_P1[proc_page_id].flags & PAGE_VALID_BIT);
  case 2:
    return (bool)(page_table_P2[proc_page_id].flags & PAGE_VALID_BIT);
  case 3:
    return (bool)(page_table_P3[proc_page_id].flags & PAGE_VALID_BIT);
  case 4:
    return (bool)(page_table_P4[proc_page_id].flags & PAGE_VALID_BIT);
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// set the page frame of the requested page
static inline void set_page_frame(const int proc_id, const int proc_page_id,
                                  const int page_frame) {
  assert((page_frame == -1) || (page_frame >= 0 && page_frame < RAM_MAX_PAGES));

  switch (proc_id) {
  case 1:
    page_table_P1[proc_page_id].page_frame = page_frame;
    break;
  case 2:
    page_table_P2[proc_page_id].page_frame = page_frame;
    break;
  case 3:
    page_table_P3[proc_page_id].page_frame = page_frame;
    break;
  case 4:
    page_table_P4[proc_page_id].page_frame = page_frame;
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// get the page frame of the requested page
static inline int get_page_frame(const int proc_id, const int proc_page_id) {
  switch (proc_id) {
  case 1:
    return page_table_P1[proc_page_id].page_frame;
  case 2:
    return page_table_P2[proc_page_id].page_frame;
  case 3:
    return page_table_P3[proc_page_id].page_frame;
  case 4:
    return page_table_P4[proc_page_id].page_frame;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// set the age bits vector of the requested page
static inline void set_age_bits(const int proc_id, const int proc_page_id,
                                page_age_bits_t age) {
  assert(algorithm == ALGO_LRU);

  switch (proc_id) {
  case 1:
    page_table_P1[proc_page_id].age_bits = age;
    break;
  case 2:
    page_table_P2[proc_page_id].age_bits = age;
    break;
  case 3:
    page_table_P3[proc_page_id].age_bits = age;
    break;
  case 4:
    page_table_P4[proc_page_id].age_bits = age;
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// get the age bits vector of the requested page
static inline page_age_bits_t get_age_bits(const int proc_id,
                                           const int proc_page_id) {
  assert(algorithm == ALGO_LRU);

  switch (proc_id) {
  case 1:
    return page_table_P1[proc_page_id].age_bits;
  case 2:
    return page_table_P2[proc_page_id].age_bits;
  case 3:
    return page_table_P3[proc_page_id].age_bits;
  case 4:
    return page_table_P4[proc_page_id].age_bits;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// shifts the aging bits in each process' page table, simulating a clock tick,
// then sets the process' age MSB according to its reference bit
static inline void shift_aging_bits(void) {
  assert(algorithm == ALGO_LRU);

  for (int i = 0; i < PROC_MAX_PAGES; i++) {
    // shift age bits
    page_table_P1[i].age_bits >>= 1;
    page_table_P2[i].age_bits >>= 1;
    page_table_P3[i].age_bits >>= 1;
    page_table_P4[i].age_bits >>= 1;

    // set MSB according to reference bit
    if (get_referenced(1, i))
      page_table_P1[i].age_bits |= 0b10000000;
    if (get_referenced(2, i))
      page_table_P2[i].age_bits |= 0b10000000;
    if (get_referenced(3, i))
      page_table_P3[i].age_bits |= 0b10000000;
    if (get_referenced(4, i))
      page_table_P4[i].age_bits |= 0b10000000;
  }
}

// set the age clock of the requested page
static inline void set_age_clock(const int proc_id, const int proc_page_id,
                                 const int age_clock) {
  assert(algorithm == ALGO_WS);

  switch (proc_id) {
  case 1:
    page_table_P1[proc_page_id].age_clock = age_clock;
    break;
  case 2:
    page_table_P2[proc_page_id].age_clock = age_clock;
    break;
  case 3:
    page_table_P3[proc_page_id].age_clock = age_clock;
    break;
  case 4:
    page_table_P4[proc_page_id].age_clock = age_clock;
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// get the age clock of the requested page
static inline int get_age_clock(const int proc_id, const int proc_page_id) {
  assert(algorithm == ALGO_WS);

  switch (proc_id) {
  case 1:
    return page_table_P1[proc_page_id].age_clock;
  case 2:
    return page_table_P2[proc_page_id].age_clock;
  case 3:
    return page_table_P3[proc_page_id].age_clock;
  case 4:
    return page_table_P4[proc_page_id].age_clock;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// get the index of the first free page frame from main memory
static int get_free_memory_index(void) {
  int free_memory = -1;

  for (int i = 0; i < RAM_MAX_PAGES; i++) {
    if (!main_memory[i]) {
      free_memory = i;
      break;
    }
  }

  // this should never be called when there are no free page frames
  assert(free_memory != -1);

  return free_memory;
}

// get ID of page to swap out of memory according to Not Recently Used,
// -1 if not found
static int get_lowest_category_page_NRU(const int proc_id) {
  // search for a page to replace according to NRU priority
  for (int i = 0; i < PROC_MAX_PAGES; i++) {
    if (get_valid(proc_id, i) && !get_referenced(proc_id, i) &&
        !get_modified(proc_id, i)) {
      return i; // UNreferenced and UNmodified
    }
  }
  for (int i = 0; i < PROC_MAX_PAGES; i++) {
    if (get_valid(proc_id, i) && !get_referenced(proc_id, i) &&
        get_modified(proc_id, i)) {
      return i; // UNreferenced and modified
    }
  }
  for (int i = 0; i < PROC_MAX_PAGES; i++) {
    if (get_valid(proc_id, i) && get_referenced(proc_id, i) &&
        !get_modified(proc_id, i)) {
      return i; // referenced and UNmodified
    }
  }
  for (int i = 0; i < PROC_MAX_PAGES; i++) {
    if (get_valid(proc_id, i) && get_referenced(proc_id, i) &&
        get_modified(proc_id, i)) {
      return i; // referenced and modified
    }
  }

  // no page found, shouldn't happen
  return -1;
}

// get the oldest page in memory for the specified process using their age bits
static int get_oldest_page_LRU(const int proc_id) {
  int oldest_page = -1;
  // the lowest age actually represents the oldest page in memory
  page_age_bits_t lowest_age = ~0;

  // find valid page with lowest age
  for (int i = 0; i < PROC_MAX_PAGES; i++) {
    page_age_bits_t age = get_age_bits(proc_id, i);

    if (get_valid(proc_id, i) && age < lowest_age) {
      oldest_page = i;
      lowest_age = age;
    }
  }

  return oldest_page;
}

// enqueue a page to the 2ndC page queue of the specified process
static inline void enqueue_page(const int proc_id, const int proc_page_id) {
  switch (proc_id) {
  case 1:
    enqueue(page_queue_P1, proc_page_id);
    break;
  case 2:
    enqueue(page_queue_P2, proc_page_id);
    break;
  case 3:
    enqueue(page_queue_P3, proc_page_id);
    break;
  case 4:
    enqueue(page_queue_P4, proc_page_id);
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

// dequeue a page from the 2ndC page queue of the specified process
static inline int dequeue_page(const int proc_id) {
  int proc_page_id = -1;

  switch (proc_id) {
  case 1:
    proc_page_id = dequeue(page_queue_P1);
    break;
  case 2:
    proc_page_id = dequeue(page_queue_P2);
    break;
  case 3:
    proc_page_id = dequeue(page_queue_P3);
    break;
  case 4:
    proc_page_id = dequeue(page_queue_P4);
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }

  assert(proc_page_id != -1); // 2ndC queue should never be empty
  return proc_page_id;
}

// handle page fault according to Not Recently Used
static void page_algo_NRU(const vmem_io_request_t req) {
  const int swap_page = get_lowest_category_page_NRU(req.proc_id);
  assert(swap_page != -1); // there should always be a page to swap
  const int swap_frame = get_page_frame(req.proc_id, swap_page);
  assert(swap_frame != -1); // page to swap should be in memory

  // check if we are swapping a modified page
  if (get_modified(req.proc_id, swap_page)) {
    // dirty
    increment_fault_count(req, true);
    msg("Page fault P%d: %02d -> frame %02d (replaced %02d) (dirty)",
        req.proc_id, req.proc_page_id, swap_frame, swap_page);
  } else {
    // clean
    increment_fault_count(req, false);
    msg("Page fault P%d: %02d -> frame %02d (replaced %02d) (clean)",
        req.proc_id, req.proc_page_id, swap_frame, swap_page);
  }

  // update page frames
  set_page_frame(req.proc_id, req.proc_page_id, swap_frame);
  set_page_frame(req.proc_id, swap_page, -1);

  // update flag bits
  set_valid(req.proc_id, req.proc_page_id, true);
  set_valid(req.proc_id, swap_page, false);
  set_referenced(req.proc_id, swap_page, false);
  set_modified(req.proc_id, swap_page, false);
}

// handle page fault according to Second Chance
static void page_algo_2ndC(const vmem_io_request_t req) {
  int oldest_page = dequeue_page(req.proc_id);
  assert(oldest_page != -1); // there should be a page in queue
  int oldest_frame;

  // find oldest page that hasn't been referenced, giving others a 2nd chance
  while (get_referenced(req.proc_id, oldest_page)) {
    set_referenced(req.proc_id, oldest_page, false);
    enqueue_page(req.proc_id, oldest_page);
    oldest_page = dequeue_page(req.proc_id);
    assert(oldest_page != -1); // there should be a page in queue
  }

  oldest_frame = get_page_frame(req.proc_id, oldest_page);
  assert(oldest_frame != -1); // oldest page should be in memory

  // check if we are swapping a modified page
  if (get_modified(req.proc_id, oldest_page)) {
    // dirty
    increment_fault_count(req, true);
    msg("Page fault P%d: %02d -> frame %02d (replaced %02d) (dirty)",
        req.proc_id, req.proc_page_id, oldest_frame, oldest_page);
  } else {
    // clean
    increment_fault_count(req, false);
    msg("Page fault P%d: %02d -> frame %02d (replaced %02d) (clean)",
        req.proc_id, req.proc_page_id, oldest_frame, oldest_page);
  }

  // enqueue newest page
  enqueue_page(req.proc_id, req.proc_page_id);

  // update page frames
  set_page_frame(req.proc_id, req.proc_page_id, oldest_frame);
  set_page_frame(req.proc_id, oldest_page, -1);

  // update flag bits
  set_valid(req.proc_id, req.proc_page_id, true);
  set_valid(req.proc_id, oldest_page, false);
  set_modified(req.proc_id, oldest_page, false);
}

// handle page fault according to LRU (Aging)
static void page_algo_LRU(const vmem_io_request_t req) {
  int oldest_page = get_oldest_page_LRU(req.proc_id);
  assert(oldest_page != -1); // there should be an oldest page
  int oldest_frame = get_page_frame(req.proc_id, oldest_page);
  assert(oldest_frame != -1); // oldest page should be in memory

  // check if we are swapping a modified page
  if (get_modified(req.proc_id, oldest_page)) {
    // dirty
    increment_fault_count(req, true);
    msg("Page fault P%d: %02d -> frame %02d (replaced %02d) (dirty)",
        req.proc_id, req.proc_page_id, oldest_frame, oldest_page);
  } else {
    // clean
    increment_fault_count(req, false);
    msg("Page fault P%d: %02d -> frame %02d (replaced %02d) (clean)",
        req.proc_id, req.proc_page_id, oldest_frame, oldest_page);
  }

  // update page frames
  set_page_frame(req.proc_id, req.proc_page_id, oldest_frame);
  set_page_frame(req.proc_id, oldest_page, -1);

  // update flag bits
  set_valid(req.proc_id, req.proc_page_id, true);
  set_valid(req.proc_id, oldest_page, false);
  set_referenced(req.proc_id, oldest_page, false);
  set_modified(req.proc_id, oldest_page, false);
  set_age_bits(req.proc_id, oldest_page, 0); // reset age
}

// handle page fault according to Working Set (k_param)
static void page_algo_WS(const vmem_io_request_t req) {
  // TODO: working set algo
}

// handle memory io request, checking if a page fault is necessary and
// updating stats as needed
static void handle_vmem_io_request(const vmem_io_request_t req) {
  // validate data coming from procs_sim
  assert(req.proc_id >= 1 && req.proc_id <= 4);
  assert(req.proc_page_id >= 0 && req.proc_page_id < PROC_MAX_PAGES);
  assert(req.operation == 'R' || req.operation == 'W');

  dmsg("vmem_sim got P%d: %02d %c", req.proc_id, req.proc_page_id,
       req.operation);

  // update flags and stats
  increment_rw_count(req);
  set_referenced(req.proc_id, req.proc_page_id, true);
  if (req.operation == 'W') {
    // page has been modified, so it must be written before being replaced
    set_modified(req.proc_id, req.proc_page_id, true);
  }

  // update pages
  if (!is_in_memory(req) && is_memory_available()) {
    // page fault, but no need to replace

    int page_frame = get_free_memory_index();

    // occupy page frame in main memory
    main_memory[page_frame] = true;
    set_valid(req.proc_id, req.proc_page_id, true);
    set_page_frame(req.proc_id, req.proc_page_id, page_frame);

    // update page fault stats
    increment_fault_count(req, false);

    if (algorithm == ALGO_2ndC) {
      // enqueue new page
      enqueue_page(req.proc_id, req.proc_page_id);
    }

    msg("Page fault P%d: %02d -> frame %02d (replaced none) (clean)",
        req.proc_id, req.proc_page_id, page_frame);
  } else if (!is_in_memory(req)) {
    // page fault, replace with selected algorithm
    page_algo_func(req);
  }
}

// print page table entries for each process, excluding statistics
static void print_page_tables(void) {
  // TODO: print page table
}

// print simulation results
static void print_stats(void) {
  int total_reads = 0, total_writes = 0, total_page_faults = 0,
      total_modified_faults = 0;
  int total_requests = 0;
  page_table_entry_t *page_tables[] = {page_table_P1, page_table_P2,
                                       page_table_P3, page_table_P4};

  // sum and print stats from each process table
  for (int p = 0; p < 4; p++) {
    int reads = 0, writes = 0, page_faults = 0, modified_faults = 0;
    for (int i = 0; i < PROC_MAX_PAGES; i++) {
      reads += page_tables[p][i].read_count;
      writes += page_tables[p][i].write_count;
      page_faults += page_tables[p][i].page_fault_count;
      modified_faults += page_tables[p][i].modified_fault_count;
    }
    total_reads += reads;
    total_writes += writes;
    total_page_faults += page_faults;
    total_modified_faults += modified_faults;
    total_requests += (reads + writes);

    msg("--- P%d Stats ---", p + 1);
    msg("Reads: %d", reads);
    msg("Writes: %d", writes);
    msg("Page Faults: %d", page_faults);
    msg("Modified Faults: %d", modified_faults);
  }

  // print combined stats
  msg("--- Combined Stats ---");
  msg("Total Reads: %d", total_reads);
  msg("Total Writes: %d", total_writes);
  msg("Total Page Faults: %d", total_page_faults);
  msg("Total Modified Faults: %d", total_modified_faults);
  msg("Total Requests: %d", total_requests);
  msg("Page Fault Rate: %.2f%%",
      (total_page_faults / (double)total_requests) * 100);
  msg("Dirty Fault Rate: %.2f%%",
      (total_modified_faults / (double)total_requests) * 100);
}

int main(int argc, char **argv) {
  dmsg("vmem_sim started");

  // parse command line args
  if (!(argc == 3 || argc == 4)) {
    fprintf(stderr, "Usage: ./vmem_sim <num_rounds> <page_algo> [<k_param>]\n");
    exit(3);
  }

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
    page_algo_func = page_algo_2ndC;
  } else if (strcasecmp(argv[2], "lru") == 0) {
    algorithm = ALGO_LRU;
    page_algo_func = page_algo_LRU;
  } else if (strcasecmp(argv[2], "ws") == 0) {
    algorithm = ALGO_WS;
    page_algo_func = page_algo_WS;
  } else {
    fprintf(stderr, "Invalid page algorithm: %s\n", argv[2]);
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

  init_page_data();

  if (algorithm == ALGO_WS) {
    msg("--- Simulating %d rounds using %s with k=%d, clear/shift every %d "
        "rounds ---",
        num_rounds, PAGE_ALGO_STR[algorithm], k_param, REF_CLEAR_INTERVAL);
  } else {
    msg("--- Simulating %d rounds using %s, clear/shift every %d rounds ---",
        num_rounds, PAGE_ALGO_STR[algorithm], REF_CLEAR_INTERVAL);
  }

  // track elapsed time
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  // main loop, post sem and read memory io requests from processes'
  // pipes. each iteration is a round, meaning one IO request from each
  // process
  for (int i = 1; i <= num_rounds; i++) {
    vmem_io_request_t req;

    // Process 1
    sem_post(sem_P1);

    if (read(pipe_P1[PIPE_READ], &req, sizeof(req)) == -1) {
      perror("Pipe read error");
      exit(9);
    }
    handle_vmem_io_request(req);

    // Process 2
    sem_post(sem_P2);

    if (read(pipe_P2[PIPE_READ], &req, sizeof(req)) == -1) {
      perror("Pipe read error");
      exit(9);
    }
    handle_vmem_io_request(req);

    // Process 3
    sem_post(sem_P3);

    if (read(pipe_P3[PIPE_READ], &req, sizeof(req)) == -1) {
      perror("Pipe read error");
      exit(9);
    }
    handle_vmem_io_request(req);

    // Process 4
    sem_post(sem_P4);

    if (read(pipe_P4[PIPE_READ], &req, sizeof(req)) == -1) {
      perror("Pipe read error");
      exit(9);
    }
    handle_vmem_io_request(req);

    // periodically shift aging and clear reference bits
    if (i % REF_CLEAR_INTERVAL == 0) {
      if (algorithm == ALGO_LRU) {
        // shift aging bits when using LRU (Aging)
        shift_aging_bits();
      }
      if (algorithm != ALGO_2ndC) {
        // clear reference bits when not using Second Chance
        clear_ref_bits();
      }
    }

    if (algorithm == ALGO_WS) {
      // update working sets and increment global age clock counter
      // TODO: update wsets

      clock_counter++;
    }

    dmsg("vmem_sim finished round %d", i);
  }

  // print results
  clock_gettime(CLOCK_MONOTONIC, &end);
  double elapsed_time_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                           (end.tv_nsec - start.tv_nsec) / 1e6;
  msg("--- Simulation finished after %dms ---", (int)(elapsed_time_ms));

  print_page_tables();
  print_stats();

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
  if (algorithm == ALGO_2ndC) {
    free_queue(page_queue_P1);
    free_queue(page_queue_P2);
    free_queue(page_queue_P3);
    free_queue(page_queue_P4);
  }
  if (algorithm == ALGO_WS) {
    free_set(page_wset_P1);
    free_set(page_wset_P2);
    free_set(page_wset_P3);
    free_set(page_wset_P4);
  }

  dmsg("vmem_sim finished");

  return EXIT_SUCCESS;
}
