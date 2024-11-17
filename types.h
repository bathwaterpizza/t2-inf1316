#pragma once

#define PIPE_READ 0
#define PIPE_WRITE 1

#define SEM_P1_NAME "/sem_P1"
#define SEM_P2_NAME "/sem_P2"
#define SEM_P3_NAME "/sem_P3"
#define SEM_P4_NAME "/sem_P4"

typedef enum {
  ALGO_NRU,  // Not Recently Used
  ALGO_2ndC, // Second Chance
  ALGO_LRU,  // Least Recently Used
  ALGO_WS    // Working Set (takes k param)
} page_algo_t;
