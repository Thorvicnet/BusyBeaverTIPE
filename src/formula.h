#ifndef FORMULA_H
#define FORMULA_H

#include "types.h"
#include <stddef.h>

LoopType formula_detect_loop(const History *history, Ins *table[2],
                             uint64_t max_period);

#endif // FORMULA_H
