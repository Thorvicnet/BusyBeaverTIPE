#include "tape.h"

Tape make_tape(uint64_t size) {
  Tape tape;
  tape.data = calloc(size, sizeof(uint8_t));
  if (!tape.data)
    exit(ENOMEM);
  tape.size = size;
  tape.up = 0;
  tape.lp = 0;
  tape.origin = 0;
  tape.min_pos = 0;
  tape.max_pos = 0;
  return tape;
}

int64_t tape_head_pos(const Tape *tape) {
  return tape->origin + (int64_t)(8 * tape->up) + (int64_t)tape->lp;
}

void tape_update_bounds(Tape *tape) {
  int64_t pos = tape_head_pos(tape);
  if (pos < tape->min_pos)
    tape->min_pos = pos;
  if (pos > tape->max_pos)
    tape->max_pos = pos;
}

Tape push_front_tape(Tape tape, uint8_t value) {
  uint8_t *new_data = realloc(tape.data, tape.size + 1);
  if (!new_data)
    exit(ENOMEM);

  memmove(new_data + 1, new_data, tape.size);
  new_data[0] = value;

  tape.data = new_data;
  tape.size++;
  tape.up++;
  tape.origin -= 8;
  return tape;
}

Tape push_back_tape(Tape tape, uint8_t value) {
  uint8_t *new_data = realloc(tape.data, tape.size + 1);
  if (!new_data)
    exit(ENOMEM);

  new_data[tape.size] = value;

  tape.data = new_data;
  tape.size++;
  return tape;
}

Tape move_left_tape(Tape tape) {
  if (tape.lp == 0) {
    if (tape.up == 0)
      tape = push_front_tape(tape, 0U);
    tape.lp = 7;
    tape.up--;
  } else {
    tape.lp--;
  }
  tape_update_bounds(&tape);
  return tape;
}

Tape move_right_tape(Tape tape) {
  if (tape.lp == 7) {
    if (tape.up == tape.size - 1)
      tape = push_back_tape(tape, 0U);
    tape.lp = 0;
    tape.up++;
  } else {
    tape.lp++;
  }
  tape_update_bounds(&tape);
  return tape;
}

Tape set_tape(Tape tape, uint8_t value) {
  tape.data[tape.up] =
      (uint8_t)((tape.data[tape.up] & ~(1U << tape.lp)) | (value << tape.lp));
  return tape;
}

uint8_t get_tape(Tape tape) {
  return (uint8_t)((tape.data[tape.up] >> tape.lp) & 1U);
}

uint8_t tape_at_pos(const Tape *tape, int64_t pos) {
  int64_t offset = pos - tape->origin;
  if (offset < 0)
    return 0;

  uint64_t bit = (uint64_t)offset;
  uint64_t byte = bit / 8;
  if (byte >= tape->size)
    return 0;

  return (uint8_t)((tape->data[byte] >> (bit % 8)) & 1U);
}

void free_tape(Tape tape) { free(tape.data); }
