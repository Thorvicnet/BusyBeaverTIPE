#ifndef BOUNCER_H
#define BOUNCER_H

#include "types.h"
#include <stdint.h>

LoopType bouncer_detect_loop(const History *history, Ins *table[2]);

#endif // BOUNCER_H
