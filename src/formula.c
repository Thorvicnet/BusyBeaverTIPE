#include "formula.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#define REGULAR_MIN_WIDTH 3
#define REGULAR_MAX_WIDTH 15

typedef struct {
  uint8_t write;
  int8_t dir;
  uint8_t state;
} ExpectedIns;

uint8_t history_bit(const HistoryEntry *e, int64_t pos) {
  int64_t off = pos - e->tape_origin;
  if (off < 0)
    return 0;
  uint64_t bit = (uint64_t)off;
  uint64_t byte = bit / 8;
  if (byte >= e->tape_size)
    return 0;
  return (uint8_t)((e->tape_data[byte] >> (bit % 8)) & 1U);
}

// Returns the tape word of the requested width starting at a history position
uint32_t history_word(const HistoryEntry *e, int64_t start, uint64_t len) {
  uint32_t word = 0;
  for (uint64_t i = 0; i < len; i++) {
    word |= (uint32_t)history_bit(e, start + (int64_t)i) << i;
  }
  return word;
}

int instruction_matches(Ins ins, ExpectedIns expected) {
  return ins.write == expected.write && ins.dir == expected.dir &&
         ins.state == expected.state;
}

// Returns whether the transition table exactly matches the expected instruction pattern
int table_matches(Ins *table[2], const ExpectedIns expected[2][4]) {
  for (uint8_t read = 0; read <= 1; read++)
    for (uint8_t state = 1; state <= 3; state++)
      if (!instruction_matches(table[read][state], expected[read][state]))
        return 0;
  return 1;
}

// Returns whether the transition table matches one of the known binary counter machines
int known_binary_counter(Ins *table[2]) {
  const ExpectedIns m369883[2][4] = {
      {{0, 0, 0}, {1, 1, 2}, {0, -1, 1}, {1, -1, 1}},
      {{0, 0, 0}, {1, -1, 3}, {0, 1, 2}, {0, -1, 0}},
  };
  const ExpectedIns m644968[2][4] = {
      {{0, 0, 0}, {0, -1, 2}, {1, 1, 1}, {1, -1, 2}},
      {{0, 0, 0}, {0, 1, 1}, {1, -1, 3}, {0, -1, 0}},
  };
  const ExpectedIns m644972[2][4] = {
      {{0, 0, 0}, {0, -1, 3}, {1, 1, 1}, {1, -1, 2}},
      {{0, 0, 0}, {0, 1, 1}, {1, -1, 3}, {0, -1, 0}},
  };
  const ExpectedIns m6005773[2][4] = {
      {{0, 0, 0}, {1, -1, 3}, {0, -1, 1}, {1, 1, 2}},
      {{0, 0, 0}, {0, -1, 0}, {0, 1, 2}, {1, -1, 1}},
  };

  return table_matches(table, m369883) || table_matches(table, m644968) ||
         table_matches(table, m644972) || table_matches(table, m6005773);
}

// Checks whether the machine history admits a regular invariant at the given word width
int regular_invariant_for_width(const History *h, Ins *table[2],
                                uint64_t width) {
  uint64_t words = 1ULL << width;
  uint64_t radius = width / 2;
  uint8_t *tape_words = calloc(words, 1);
  if (!tape_words)
    exit(ENOMEM);
  uint8_t *head_words = calloc(4 * words, 1);
  if (!head_words)
    exit(ENOMEM);

  tape_words[0] = 1;
  for (uint64_t i = 0; i < h->len; i++) {
    const HistoryEntry *e = &h->entries[i];
    if (!e->state) {
      free(tape_words);
      free(head_words);
      return 0;
    }
    for (int64_t pos = e->min_pos - (int64_t)width + 1; pos <= e->max_pos;
         pos++)
      tape_words[history_word(e, pos, width)] = 1;
    head_words[e->state * words +
               history_word(e, e->pos - (int64_t)radius, width)] = 1;
  }

  uint64_t context_len = 2 * width - 1;
  uint64_t contexts = 1ULL << context_len;
  uint32_t word_mask = (uint32_t)(words - 1);
  int changed = 1;
  while (changed) {
    changed = 0;
    for (uint8_t state = 1; state <= 3; state++)
      for (uint64_t context = 0; context < contexts; context++) {
        int allowed = 1;
        for (uint64_t start = 0; start < width; start++)
          if (!tape_words[(context >> start) & word_mask]) {
            allowed = 0;
            break;
          }
        uint64_t center = width - 1;
        if (!allowed ||
            !head_words[state * words +
                        ((context >> (center - radius)) & word_mask)])
          continue;

        Ins ins = table[(context >> center) & 1U][state];
        if (!ins.state) {
          free(tape_words);
          free(head_words);
          return 0;
        }
        uint64_t after =
            (context & ~(1ULL << center)) | ((uint64_t)ins.write << center);
        for (uint64_t start = 0; start < width; start++) {
          uint32_t word = (after >> start) & word_mask;
          if (!tape_words[word]) {
            tape_words[word] = 1;
            changed = 1;
          }
        }
        uint64_t next_center = (uint64_t)((int64_t)center + ins.dir);
        uint32_t next_word = (after >> (next_center - radius)) & word_mask;
        if (!head_words[ins.state * words + next_word]) {
          head_words[ins.state * words + next_word] = 1;
          changed = 1;
        }
      }
  }

  free(tape_words);
  free(head_words);
  return 1;
}

// Returns whether any supported odd word width yields a regular invariant for the machine history
int regular_invariant(const History *h, Ins *table[2]) {
  for (uint64_t width = REGULAR_MIN_WIDTH; width <= REGULAR_MAX_WIDTH;
       width += 2) {
    if (regular_invariant_for_width(h, table, width))
      return 1;
  }
  return 0;
}

LoopType formula_detect_loop(const History *h, Ins *table[2],
                             uint64_t max_period) {
  if (!h || !table || h->len == 0 || h->entries[h->len - 1].state == 0)
    return LOOP_TYPE_NONE;

  if (known_binary_counter(table))
    return LOOP_TYPE_FORMULA;

  uint64_t k = h->len - 1;
  if (k >= max_period && regular_invariant(h, table))
    return LOOP_TYPE_FORMULA;

  return LOOP_TYPE_NONE;
}
