#pragma once

#include "types.h"

// unbuffered printf + timestamp
void msg(const char *format, ...);

// unbuffered printf + timestamp for DEBUG only
void dmsg(const char *format, ...);
