#include "util.h"
#include "types.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// documentation is provided in util.h

void msg(const char *format, ...) {
  struct timespec ts;
  va_list args;

  // Get current time with nanosecond precision
  clock_gettime(CLOCK_REALTIME, &ts);

  // Extract hours, minutes and seconds
  struct tm tm_info;
  localtime_r(&ts.tv_sec, &tm_info);

  // Print timestamp with hours, minutes, seconds, and milliseconds
  printf("[%02d:%02d:%02d.%02ld] ", tm_info.tm_hour, tm_info.tm_min,
         tm_info.tm_sec, ts.tv_nsec / 10000000);

  // Print the rest of the message
  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  printf("\n");

  // Flush the output to ensure it's displayed immediately
  fflush(stdout);
}

void dmsg(const char *format, ...) {
#ifdef DEBUG
  struct timespec ts;
  va_list args;

  // Get current time with nanosecond precision
  clock_gettime(CLOCK_REALTIME, &ts);

  // Extract hours, minutes and seconds
  struct tm tm_info;
  localtime_r(&ts.tv_sec, &tm_info);

  // Print timestamp with hours, minutes, seconds, and milliseconds
  printf("[%02d:%02d:%02d.%02ld] ", tm_info.tm_hour, tm_info.tm_min,
         tm_info.tm_sec, ts.tv_nsec / 10000000);

  // Print the rest of the message
  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  printf("\n");

  // Flush the output to ensure it's displayed immediately
  fflush(stdout);
#endif
}

queue_t *create_queue(void) {
  queue_t *q = (queue_t *)malloc(sizeof(queue_t));
  if (q == NULL) {
    fprintf(stderr, "Malloc error\n");
    exit(6);
  }

  q->front = q->rear = NULL;

  return q;
}

void free_queue(queue_t *q) {
  node_t *current = q->front;
  node_t *next;

  while (current != NULL) {
    next = current->next;
    free(current);
    current = next;
  }

  free(q);
}

void enqueue(queue_t *q, int value) {
  node_t *temp = (node_t *)malloc(sizeof(node_t));
  if (temp == NULL) {
    fprintf(stderr, "Malloc error\n");
    exit(6);
  }
  temp->data = value;
  temp->next = NULL;

  if (q->rear == NULL) {
    q->front = q->rear = temp;
    return;
  }

  q->rear->next = temp;
  q->rear = temp;
}

int dequeue(queue_t *q) {
  if (q->front == NULL)
    return -1;

  node_t *temp = q->front;
  int value = temp->data;

  q->front = q->front->next;

  if (q->front == NULL)
    q->rear = NULL;

  free(temp);

  return value;
}
