#pragma once

#include "types.h"

// unbuffered printf + timestamp, includes newline
void msg(const char *format, ...);

// unbuffered printf + timestamp for DEBUG only, includes newline
void dmsg(const char *format, ...);
