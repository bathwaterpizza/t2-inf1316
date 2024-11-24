CC = gcc
CFLAGS = -Wall -g

# List of all programs
PROGRAMS = pagelist_gen vmem_sim procs_sim

# Common source files
COMMON_SRC = util.c types.c

# Header files
HEADERS = util.h types.h

# Default target
all: $(PROGRAMS)

# Rule for pagelist_gen
pagelist_gen: pagelist_gen.c $(COMMON_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ pagelist_gen.c $(COMMON_SRC)

# Rule for vmem_sim
vmem_sim: vmem_sim.c $(COMMON_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ vmem_sim.c $(COMMON_SRC)

# Rule for procs_sim
procs_sim: procs_sim.c $(COMMON_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ procs_sim.c $(COMMON_SRC)

# Clean up build artifacts
clean:
	rm -f $(PROGRAMS)

# Phony targets
.PHONY: all clean
