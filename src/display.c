#include "encoding.h"
#include "types.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

const uint64_t DISPLAY_STEPS = 64;

void print_tape(const uint8_t *tape, uint64_t len, uint64_t step) {
  printf("%4llu ", (unsigned long long)step);
  for (uint64_t i = 0; i < len; i++)
    putchar((char)('0' + tape[i]));
  putchar('\n');
}

int main(int argc, char **argv) {
  Ins zero_row[4];
  Ins one_row[4];
  Ins *table[2] = {zero_row, one_row};
  build_machine(strtoull(argv[1], NULL, 0), zero_row, one_row);

  uint64_t len = (uint64_t)(2 * DISPLAY_STEPS + 1);
  uint64_t head = (uint64_t)DISPLAY_STEPS;
  uint8_t *tape = calloc(len, sizeof(*tape));
  if (!tape)
    return EXIT_FAILURE;

  uint8_t state = 1;
  print_tape(tape, len, 0);

  for (uint64_t step = 1; step <= DISPLAY_STEPS && state; step++) {
    Ins ins = table[tape[head]][state];
    tape[head] = ins.write;
    head = (uint64_t)((int64_t)head + ins.dir);
    state = ins.state;
    print_tape(tape, len, step);
  }

  free(tape);
  return EXIT_SUCCESS;
}
