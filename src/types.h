#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

typedef struct {
  uint8_t *data;
  uint64_t size;

  // Tape coordinates.
  // Absolute head position = origin + 8 * up + lp.
  uint64_t up;
  uint8_t lp;
  int64_t origin;

  int64_t min_pos;
  int64_t max_pos;
} Tape;

typedef struct {
  uint8_t write;
  int8_t dir;    // dir = 1 means R, anything else means L
  uint8_t state; // 0 is halt, execution begins in state 1
} Ins;

typedef struct {
  Tape tape;
  uint8_t state;
  Ins *table[2];
} Beaver;

typedef enum {
  LOOP_TYPE_NONE = 0,
  LOOP_TYPE_EXACT = 1,
  LOOP_TYPE_DRIFT_POSITIVE = 2,
  LOOP_TYPE_DRIFT_NEGATIVE = 3,
  LOOP_TYPE_FORMULA = 4
} LoopType;

typedef enum {
  MACHINE_RESULT_TYPE_HALTED = 0,
  MACHINE_RESULT_TYPE_NONHALT_LOOP = 1,
  MACHINE_RESULT_TYPE_UNKNOWN = 2
} MachineResultType;

typedef struct {
  uint8_t state;
  uint8_t read;
  int64_t pos;
  int64_t min_pos;
  int64_t max_pos;
  uint8_t *tape_data;
  uint64_t tape_size;
  int64_t tape_origin;
} HistoryEntry;

typedef struct {
  HistoryEntry *entries;
  uint64_t len;
  uint64_t cap;
} History;

typedef struct {
  MachineResultType type;
  LoopType loop;
  uint64_t steps;
} MachineResult;

#define STARTING_TAPE_SIZE 8
#define STARTING_HISTORY_SIZE 1024
#define STEP_LIMIT 100
#define MAX_PERIOD 100

#endif // TYPES_H
