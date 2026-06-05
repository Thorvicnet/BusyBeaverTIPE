#ifndef TAPE_H
#define TAPE_H

#include "types.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

Tape make_tape(uint64_t size);
Tape push_front_tape(Tape tape, uint8_t value);
Tape push_back_tape(Tape tape, uint8_t value);
Tape move_left_tape(Tape tape);
Tape move_right_tape(Tape tape);
Tape set_tape(Tape tape, uint8_t value);
uint8_t get_tape(Tape tape);
uint8_t tape_at_pos(const Tape *tape, int64_t pos);
int64_t tape_head_pos(const Tape *tape);
void tape_update_bounds(Tape *tape);
void free_tape(Tape tape);

#endif // TAPE_H
