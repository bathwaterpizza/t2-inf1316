#pragma once

#include "types.h"
#include <stdbool.h>

// returns whether the requested page is in memory, by checking the valid bit
bool is_in_memory(const vmem_io_request_t req);

// returns whether there is memory available to store a new page
bool is_memory_available(void);

// increments the read or write count of the requested page
void increment_rw_count(const vmem_io_request_t req);

// increments the page fault count of the requested page
void increment_fault_count(const vmem_io_request_t req, const bool is_modified);

// set or clear the modified bit of the requested page
void set_modified(const int proc_id, const int proc_page_id, const bool value);

// get the modified bit of the requested page
bool get_modified(const int proc_id, const int proc_page_id);

// set or clear the referenced bit of the requested page
void set_referenced(const int proc_id, const int proc_page_id,
                    const bool value);

// get the referenced bit of the requested page
bool get_referenced(const int proc_id, const int proc_page_id);

// set or clear the valid bit of the requested page
void set_valid(const int proc_id, const int proc_page_id, const bool value);

// get the valid bit of the requested page
bool get_valid(const int proc_id, const int proc_page_id);

// set the page frame of the requested page
void set_page_frame(const int proc_id, const int proc_page_id,
                    const int page_frame);

// get the page frame of the requested page
int get_page_frame(const int proc_id, const int proc_page_id);

// set the age bits vector of the requested page
void set_age_bits(const int proc_id, const int proc_page_id,
                  page_age_bits_t age);

// get the age bits vector of the requested page
page_age_bits_t get_age_bits(const int proc_id, const int proc_page_id);

// set the age clock of the requested page
void set_age_clock(const int proc_id, const int proc_page_id,
                   const int age_clock);

// get the age clock of the requested page
int get_age_clock(const int proc_id, const int proc_page_id);

// enqueue a page to the 2ndC page queue of the specified process
void enqueue_page(const int proc_id, const int proc_page_id);

// dequeue a page from the 2ndC page queue of the specified process
int dequeue_page(const int proc_id);

// add a page to the working set of the specified process
void set_add_page(const int proc_id, const int proc_page_id);

// remove a page from the working set of the specified process
void set_remove_page(const int proc_id, const int proc_page_id);

// returns whether the working set of proc_id contains the specified page
bool set_contains_page(const int proc_id, const int proc_page_id);

// get the queue for the specified process
queue_t *get_queue(const int proc_id);

// get the working set for the specified process
set_t *get_set(const int proc_id);
