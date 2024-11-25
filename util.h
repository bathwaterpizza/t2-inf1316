#pragma once

#include "types.h"

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
