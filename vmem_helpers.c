#include "vmem_helpers.h"
#include "types.h"
#include "util.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// documentation is provided in vmem_helpers.h

extern page_algo_t algorithm;
extern page_algo_func_t page_algo_func;
extern int k_param;
extern bool main_memory[RAM_MAX_PAGES];
extern page_table_entry_t page_table_P1[PROC_MAX_PAGES];
extern page_table_entry_t page_table_P2[PROC_MAX_PAGES];
extern page_table_entry_t page_table_P3[PROC_MAX_PAGES];
extern page_table_entry_t page_table_P4[PROC_MAX_PAGES];
extern queue_t *page_queue_P1;
extern queue_t *page_queue_P2;
extern queue_t *page_queue_P3;
extern queue_t *page_queue_P4;
extern set_t *page_wset_P1;
extern set_t *page_wset_P2;
extern set_t *page_wset_P3;
extern set_t *page_wset_P4;
extern int clock_counter;
extern bool wset_check_performed;

bool is_in_memory(const vmem_io_request_t req) {
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

bool is_memory_available(void) {
  for (int i = 0; i < RAM_MAX_PAGES; i++) {
    if (!main_memory[i])
      return true;
  }

  return false;
}

void increment_rw_count(const vmem_io_request_t req) {
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

void increment_fault_count(const vmem_io_request_t req,
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

void set_modified(const int proc_id, const int proc_page_id, const bool value) {
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

bool get_modified(const int proc_id, const int proc_page_id) {
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

void set_referenced(const int proc_id, const int proc_page_id,
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

bool get_referenced(const int proc_id, const int proc_page_id) {
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

void set_valid(const int proc_id, const int proc_page_id, const bool value) {
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

bool get_valid(const int proc_id, const int proc_page_id) {
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

void set_page_frame(const int proc_id, const int proc_page_id,
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

int get_page_frame(const int proc_id, const int proc_page_id) {
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

void set_age_bits(const int proc_id, const int proc_page_id,
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

page_age_bits_t get_age_bits(const int proc_id, const int proc_page_id) {
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

void set_age_clock(const int proc_id, const int proc_page_id,
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

int get_age_clock(const int proc_id, const int proc_page_id) {
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

void enqueue_page(const int proc_id, const int proc_page_id) {
  assert(algorithm == ALGO_2ndC);

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

int dequeue_page(const int proc_id) {
  assert(algorithm == ALGO_2ndC);
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

void set_add_page(const int proc_id, const int proc_page_id) {
  assert(algorithm == ALGO_WS);

  switch (proc_id) {
  case 1:
    set_add(page_wset_P1, proc_page_id);
    break;
  case 2:
    set_add(page_wset_P2, proc_page_id);
    break;
  case 3:
    set_add(page_wset_P3, proc_page_id);
    break;
  case 4:
    set_add(page_wset_P4, proc_page_id);
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

void set_remove_page(const int proc_id, const int proc_page_id) {
  assert(algorithm == ALGO_WS);

  switch (proc_id) {
  case 1:
    set_remove(page_wset_P1, proc_page_id);
    break;
  case 2:
    set_remove(page_wset_P2, proc_page_id);
    break;
  case 3:
    set_remove(page_wset_P3, proc_page_id);
    break;
  case 4:
    set_remove(page_wset_P4, proc_page_id);
    break;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

bool set_contains_page(const int proc_id, const int proc_page_id) {
  assert(algorithm == ALGO_WS);

  switch (proc_id) {
  case 1:
    return set_contains(page_wset_P1, proc_page_id);
  case 2:
    return set_contains(page_wset_P2, proc_page_id);
  case 3:
    return set_contains(page_wset_P3, proc_page_id);
  case 4:
    return set_contains(page_wset_P4, proc_page_id);
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

queue_t *get_queue(const int proc_id) {
  assert(algorithm == ALGO_2ndC);

  switch (proc_id) {
  case 1:
    return page_queue_P1;
  case 2:
    return page_queue_P2;
  case 3:
    return page_queue_P3;
  case 4:
    return page_queue_P4;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}

set_t *get_set(const int proc_id) {
  assert(algorithm == ALGO_WS);

  switch (proc_id) {
  case 1:
    return page_wset_P1;
  case 2:
    return page_wset_P2;
  case 3:
    return page_wset_P3;
  case 4:
    return page_wset_P4;
  default:
    fprintf(stderr, "Invalid process ID: %d\n", proc_id);
    exit(10);
  }
}
