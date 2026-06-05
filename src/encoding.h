#ifndef ENCODING_H
#define ENCODING_H

#include "types.h"
#include <stdint.h>

Ins decode_instruction(uint8_t code);
void build_machine(uint64_t id, Ins zero_row[4], Ins one_row[4]);
int reachable_states_avoid_halt_from(Ins *table[2], uint8_t start);

#endif // ENCODING_H
