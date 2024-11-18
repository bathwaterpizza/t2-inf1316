#include "util.h"
#include "types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

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
