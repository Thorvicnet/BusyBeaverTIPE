#include "beaver.h"
#include "bouncer.h"
#include "drift.h"
#include "formula.h"
#include "encoding.h"
#include "tape.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>

Beaver make_beaver(Ins *table[2]) {
  return (Beaver){
      .tape = make_tape(STARTING_TAPE_SIZE),
      .state = 1,
      .table[0] = table[0],
      .table[1] = table[1],
  };
}

void free_beaver(Beaver b) { free_tape(b.tape); }

Beaver step_beaver(Beaver b) {
  if (b.state == 0)
    return b;

  uint8_t cbit = get_tape(b.tape);
  Ins cins = b.table[cbit][b.state];

  b.tape = set_tape(b.tape, cins.write);
  b.state = cins.state;

  if (cins.dir == 1)
    b.tape = move_right_tape(b.tape);
  else
    b.tape = move_left_tape(b.tape);

  return b;
}

MachineResult run_machine(Ins *table[2]) {
  Beaver b = make_beaver(table);
  History history = history_create(STARTING_HISTORY_SIZE);

  history_append(&history, b.state, &b.tape);

  uint64_t steps = 0;
  MachineResult result = {
      .type = MACHINE_RESULT_TYPE_UNKNOWN, .loop = LOOP_TYPE_NONE, .steps = 0};

  while (steps < STEP_LIMIT) {
    if (b.state == 0) {
      result = (MachineResult){.type = MACHINE_RESULT_TYPE_HALTED,
                               .loop = LOOP_TYPE_NONE,
                               .steps = steps};
      break;
    }

    LoopType bouncer = bouncer_detect_loop(&history, table);
    if (bouncer != LOOP_TYPE_NONE) {
      result = (MachineResult){.type = MACHINE_RESULT_TYPE_NONHALT_LOOP,
                               .loop = bouncer,
                               .steps = steps};
      break;
    }

    if (reachable_states_avoid_halt_from(table, b.state)) {
      result = (MachineResult){.type = MACHINE_RESULT_TYPE_NONHALT_LOOP,
                               .loop = LOOP_TYPE_FORMULA,
                               .steps = steps};
      break;
    }

    b = step_beaver(b);
    steps++;

    history_append(&history, b.state, &b.tape);

    if (b.state == 0) {
      result = (MachineResult){.type = MACHINE_RESULT_TYPE_HALTED,
                               .loop = LOOP_TYPE_NONE,
                               .steps = steps};
      break;
    }

    LoopType loop = history_detect_loop(&history, MAX_PERIOD);
    if (loop == LOOP_TYPE_NONE && steps == STEP_LIMIT)
      loop = bouncer_detect_loop(&history, table);
    if (loop == LOOP_TYPE_NONE &&
        (steps == STEP_LIMIT || steps % 4 == 0))
      loop = formula_detect_loop(&history, table, MAX_PERIOD);
    if (loop != LOOP_TYPE_NONE) {
      result = (MachineResult){.type = MACHINE_RESULT_TYPE_NONHALT_LOOP,
                               .loop = loop,
                               .steps = steps};
      break;
    }
  }

  if (result.type == MACHINE_RESULT_TYPE_UNKNOWN)
    result.steps = steps;

  history_free(&history);
  free_beaver(b);
  return result;
}

void print_result(MachineResult result) {
  const char *loop = "unknown loop";

  switch (result.type) {
  case MACHINE_RESULT_TYPE_HALTED:
    printf("Machine halted. Steps: %llu\n", (unsigned long long)result.steps);
    break;
  case MACHINE_RESULT_TYPE_NONHALT_LOOP:
    switch (result.loop) {
    case LOOP_TYPE_NONE:
      loop = "no loop";
      break;
    case LOOP_TYPE_EXACT:
      loop = "exact cycle";
      break;
    case LOOP_TYPE_DRIFT_POSITIVE:
      loop = "positive drift";
      break;
    case LOOP_TYPE_DRIFT_NEGATIVE:
      loop = "negative drift";
      break;
    case LOOP_TYPE_FORMULA:
      loop = "formula";
      break;
    case LOOP_TYPE_BOUNCER:
      loop = "bouncer";
      break;
    default:
      break;
    }
    printf("%s detected: machine does not halt. Steps: %llu\n", loop,
           (unsigned long long)result.steps);
    break;
  case MACHINE_RESULT_TYPE_UNKNOWN:
    printf("Unknown. Steps: %llu\n", (unsigned long long)result.steps);
    break;
  default:
    printf("Impossible\n");
    break;
  }
}
