#pragma once

#include "types.h"
#include <stdbool.h>
#include <stdlib.h>

// unbuffered printf + timestamp, includes newline
void msg(const char *format, ...);

// unbuffered printf + timestamp for DEBUG only, includes newline
void dmsg(const char *format, ...);

// allocate a queue
queue_t *create_queue(void);

// free an allocated queue
void free_queue(queue_t *q);

// enqueue int
void enqueue(queue_t *q, int value);

// dequeue int
int dequeue(queue_t *q);

// print queue contents to a string buffer,
// format: int1, int2, int3.. (no newline, no leading/trailing comma)
void queue_to_str(queue_t *q, char *buffer, size_t buffer_size);

// allocate a new set that can represent elements 0-31
set_t *create_set(void);

// free a set allocated with create_set
void free_set(set_t *set);

// add value to set, duplicates are ignored
void set_add(set_t *set, int value);

// remove value from set
void set_remove(set_t *set, int value);

// check if value is in the given set
bool set_contains(const set_t *set, int value);

// print set contents to a string buffer,
// format: int1, int2, int3.. (no newline, no leading/trailing comma)
void set_to_str(const set_t *set, char *buffer, size_t buffer_size);
