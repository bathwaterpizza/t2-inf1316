#include "types.h"
#include "util.h"
#include "vmem_helpers.h"
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
page_algo_t algorithm;
// pointer to the selected page replacement algorithm's function
page_algo_func_t page_algo_func;
// working set window parameter
int k_param;
// page frames available in main memory.
// false = available, true = occupied
bool main_memory[RAM_MAX_PAGES] = {false};
// process page tables
page_table_entry_t page_table_P1[PROC_MAX_PAGES];
page_table_entry_t page_table_P2[PROC_MAX_PAGES];
page_table_entry_t page_table_P3[PROC_MAX_PAGES];
page_table_entry_t page_table_P4[PROC_MAX_PAGES];
// process page queues for Second Chance
queue_t *page_queue_P1;
queue_t *page_queue_P2;
queue_t *page_queue_P3;
queue_t *page_queue_P4;
// process working sets for Working Set(k)
set_t *page_wset_P1;
set_t *page_wset_P2;
set_t *page_wset_P3;
set_t *page_wset_P4;
// global clock time for Working Set(k) page age comparison,
// incremented every round
int clock_counter;
// whether we've checked that running WS(k) for the given k_param is possible,
// once main memory is fully occupied
bool wset_check_performed;

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
      wset_check_performed = false;
      clock_counter = 0;

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

// find a valid page that is not in the process' working set
static int get_page_outside_WS(const int proc_id) {
  for (int i = 0; i < PROC_MAX_PAGES; i++) {
    if (get_valid(proc_id, i) && !set_contains_page(proc_id, i)) {
      return i;
    }
  }

  // no page outside WS found, shouldn't happen
  return -1;
}

// update the working sets of each process according to the k_param
static void update_working_sets(void) {
  assert(algorithm == ALGO_WS);

  // for each process, go through all pages and use their age clock to check if
  // they should be in the process' working set
  for (int proc_id = 1; proc_id <= 4; proc_id++) {
    for (int page_id = 0; page_id < PROC_MAX_PAGES; page_id++) {
      if (get_valid(proc_id, page_id) &&
          (clock_counter - k_param) < get_age_clock(proc_id, page_id)) {
        // if last page ref was less than k_param clock ticks ago, add to wset
        set_add_page(proc_id, page_id);
      } else {
        // otherwise, remove from wset
        set_remove_page(proc_id, page_id);
      }
    }
  }
}

// increment page fault count considering modified pages
static inline void increment_fault(const vmem_io_request_t req, const int frame,
                                   const int page) {
  if (get_modified(req.proc_id, page)) {
    // dirty
    increment_fault_count(req, true);
    msg("Page fault P%d: %02d -> frame %02d (replaced %02d) (dirty)",
        req.proc_id, req.proc_page_id, frame, page);
  } else {
    // clean
    increment_fault_count(req, false);
    msg("Page fault P%d: %02d -> frame %02d (replaced %02d) (clean)",
        req.proc_id, req.proc_page_id, frame, page);
  }
}

// handle page fault according to Not Recently Used
static void page_algo_NRU(const vmem_io_request_t req) {
  const int swap_page = get_lowest_category_page_NRU(req.proc_id);
  assert(swap_page != -1); // there should always be a page to swap
  const int swap_frame = get_page_frame(req.proc_id, swap_page);
  assert(swap_frame != -1); // page to swap should be in memory

  // check if we are swapping a modified page
  increment_fault(req, swap_frame, swap_page);

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
  increment_fault(req, oldest_frame, oldest_page);

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
  increment_fault(req, oldest_frame, oldest_page);

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
  // because of our WS(k) viability check in handle_vmem_io_request, we know
  // that there will always be at least one page outside the working set, but
  // still in main memory, to be replaced
  int outside_page = get_page_outside_WS(req.proc_id);
  assert(outside_page != -1); // there should be a page outside the WS
  int outside_frame = get_page_frame(req.proc_id, outside_page);
  assert(outside_frame != -1); // outside page should be in memory

  // check if we are swapping a modified page
  increment_fault(req, outside_frame, outside_page);

  // update page frames
  set_page_frame(req.proc_id, req.proc_page_id, outside_frame);
  set_page_frame(req.proc_id, outside_page, -1);

  // update flag bits
  set_valid(req.proc_id, req.proc_page_id, true);
  set_valid(req.proc_id, outside_page, false);
  set_referenced(req.proc_id, outside_page, false);
  set_modified(req.proc_id, outside_page, false);

  // reset age clock
  set_age_clock(req.proc_id, outside_page, 0);
}

// handle memory io request from procs_sim, checking if a page fault is
// necessary and updating page data structures as needed
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

  if (algorithm == ALGO_WS) {
    if (!wset_check_performed && !is_memory_available()) {
      // once main memory is full, check if we can run WS(k) for the given
      // k_param. k must be less than the minimum number of page frames
      // that a process has occupied
      dmsg("Main memory is now full, checking WS(%d) viability", k_param);

      if (k_param >= get_min_page_frames()) {
        fprintf(stderr,
                "Error: k_param %d is too large for this pagelist's memory "
                "distribution, minimum frame count is %d\n",
                k_param, get_min_page_frames());
        exit(11);
      }

      wset_check_performed = true;
    }

    // update age clock
    set_age_clock(req.proc_id, req.proc_page_id, clock_counter);
  }

  // check if a page fault occurred
  if (!is_in_memory(req) && is_memory_available()) {
    // page fault, but no need to replace a page

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
    // page fault, replace a page (from the same process) with the selected
    // algorithm
    page_algo_func(req);
  }
}

// print the bit vector representation of page_flags_t to a buffer
static void flags_to_str(page_flags_t flags, char *buffer, size_t buffer_size) {
  int num_bits = sizeof(page_flags_t) * 8;

  // verify buffer size
  if (buffer_size < num_bits + 1) { // +1 for null terminator
    if (buffer_size > 0) {
      buffer[0] = '\0'; // error, set empty str
    }

    return;
  }

  // write bits as chars
  for (int i = num_bits - 1; i >= 0; i--) {
    buffer[num_bits - 1 - i] = ((flags >> i) & 1) ? '1' : '0';
  }

  buffer[num_bits] = '\0';
}

// print the bit vector representation of page_age_bits_t to a buffer
static void age_bits_to_str(page_age_bits_t age_bits, char *buffer,
                            size_t buffer_size) {
  int num_bits = sizeof(page_age_bits_t) * 8;

  // verify buffer size
  if (buffer_size < num_bits + 1) { // +1 for null terminator
    if (buffer_size > 0) {
      buffer[0] = '\0'; // error, set empty str
    }

    return;
  }

  // write bits as chars
  for (int i = num_bits - 1; i >= 0; i--) {
    buffer[num_bits - 1 - i] = ((age_bits >> i) & 1) ? '1' : '0';
  }

  buffer[num_bits] = '\0';
}

// print page table entries for each process, as well as the relevant data
// structures, excluding statistics
static void print_page_tables(void) {
  char buffer[1024];
  char flags_str[sizeof(page_flags_t) * 8 + 1];
  char age_bits_str[sizeof(page_age_bits_t) * 8 + 1];

  for (int proc_id = 1; proc_id <= 4; proc_id++) {
    page_table_entry_t *page_table;

    switch (proc_id) {
    case 1:
      page_table = page_table_P1;
      break;
    case 2:
      page_table = page_table_P2;
      break;
    case 3:
      page_table = page_table_P3;
      break;
    case 4:
      page_table = page_table_P4;
      break;
    default:
      fprintf(stderr, "Invalid process ID: %d\n", proc_id);
      exit(10);
    }

    // print page table entries
    putchar('\n');
    msg("--- P%d page table ---", proc_id);
    for (int i = 0; i < PROC_MAX_PAGES; i++) {
      char modified = get_modified(proc_id, i) ? 'M' : '-';
      char referenced = get_referenced(proc_id, i) ? 'R' : '-';
      char valid = get_valid(proc_id, i) ? 'V' : '-';
      flags_to_str(page_table[i].flags, flags_str, sizeof(flags_str));

      if (algorithm == ALGO_LRU) {
        age_bits_to_str(page_table[i].age_bits, age_bits_str,
                        sizeof(age_bits_str));

        msg("Page %02d: Frame %02d | Flags %s (%c%c%c) | Age bits %s",
            page_table[i].page_id, page_table[i].page_frame, flags_str,
            modified, referenced, valid, age_bits_str);
      } else if (algorithm == ALGO_WS) {
        msg("Page %02d: Frame %02d | Flags %s (%c%c%c) | Age clock %d",
            page_table[i].page_id, page_table[i].page_frame, flags_str,
            modified, referenced, valid, page_table[i].age_clock);
      } else {
        msg("Page %02d: Frame %02d | Flags %s (%c%c%c)", page_table[i].page_id,
            page_table[i].page_frame, flags_str, modified, referenced, valid);
      }
    }

    // print additional data structures
    if (algorithm == ALGO_2ndC) {
      queue_to_str(get_queue(proc_id), buffer, sizeof(buffer));
      msg("Process FIFO Queue: %s", buffer);
    } else if (algorithm == ALGO_WS) {
      set_to_str(get_set(proc_id), buffer, sizeof(buffer));
      msg("Process Working Set: %s", buffer);
    }

    msg("Process page frame count: %d\n", get_amount_page_frames(proc_id));
  }
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

    putchar('\n');
    msg("--- P%d Stats ---", p + 1);
    msg("Reads:             %11d", reads);
    msg("Writes:            %11d", writes);
    msg("Page Faults:       %11d", page_faults);
    msg("Modified Faults:   %11d", modified_faults);
  }

  // print combined stats
  putchar('\n');
  msg("--- Combined Stats ---");
  msg("Total Reads:       %11d", total_reads);
  msg("Total Writes:      %11d", total_writes);
  msg("Total Page Faults: %11d", total_page_faults);
  msg("Total Modified Faults: %7d", total_modified_faults);
  msg("Total Requests:    %11d", total_requests);
  msg("Page Fault Rate:   %10.2f%%",
      (total_page_faults / (double)total_requests) * 100);
  msg("Dirty Fault Rate:  %10.2f%%",
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

    if (argc != 4) {
      fprintf(stderr, "Error: Working Set algorithm requires a k parameter\n");
      fprintf(stderr,
              "Usage: ./vmem_sim <num_rounds> <page_algo> [<k_param>]\n");
      exit(3);
    }
  } else {
    fprintf(stderr, "Error: Invalid page algorithm %s\n", argv[2]);
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

    if (algorithm == ALGO_LRU) {
      // shift aging bits after each round, and clear ref bits
      shift_aging_bits();
      clear_ref_bits();
    } else if (i % REF_CLEAR_INTERVAL == 0 && algorithm != ALGO_2ndC) {
      // periodically clear reference bits
      clear_ref_bits();
    }

    if (algorithm == ALGO_WS) {
      // update working sets and increment global clock counter
      update_working_sets();
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
