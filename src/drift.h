#ifndef DRIFT_H
#define DRIFT_H

#include "types.h"
#include <stddef.h>
#include <stdint.h>

History history_create(uint64_t initial_cap);
void history_free(History *history);
void history_append(History *history, uint8_t state, const Tape *tape);
LoopType history_detect_loop(const History *history, uint64_t max_period);

#endif // DRIFT_H
