#include "encoding.h"

Ins decode_instruction(uint8_t code) {
  return (Ins){
      .write = (uint8_t)(code & 1U),
      .dir = (code & 2U) ? 1 : -1,
      .state = (uint8_t)((code >> 2) & 3U),
  };
}

void build_machine(uint64_t id, Ins zero_row[4], Ins one_row[4]) {
  zero_row[0] = (Ins){0, 0, 0};
  one_row[0] = (Ins){0, 0, 0};
  for (uint8_t state = 1; state <= 3; state++) {
    zero_row[state] = decode_instruction((uint8_t)(id & 0xFU));
    id >>= 4;
    one_row[state] = decode_instruction((uint8_t)(id & 0xFU));
    id >>= 4;
  }
}

int reachable_states_avoid_halt_from(Ins *table[2], uint8_t start) {
  if (!start)
    return 0;

  uint8_t reachable[4] = {0};
  reachable[start] = 1;

  int changed = 1;
  while (changed) {
    changed = 0;
    for (uint8_t state = 1; state <= 3; state++) {
      if (!reachable[state])
        continue;

      for (uint8_t read = 0; read <= 1; read++) {
        uint8_t next = table[read][state].state;
        if (!next)
          return 0;
        if (next <= 3 && !reachable[next]) {
          reachable[next] = 1;
          changed = 1;
        }
      }
    }
  }

  return 1;
}
