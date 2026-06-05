#include "drift.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int same_transcript_step(const History *h, uint64_t i, uint64_t j) {
  return h->entries[i].state == h->entries[j].state &&
         h->entries[i].read == h->entries[j].read;
}

int same_transcript_block(const History *h, uint64_t i, uint64_t j,
                          uint64_t period) {
  for (uint64_t k = 0; k < period; k++)
    if (!same_transcript_step(h, i + k, j + k))
      return 0;
  return 1;
}

int constant_shift_block(const History *h, uint64_t i, uint64_t j,
                         uint64_t period, int64_t *shift_out) {
  int64_t shift = h->entries[j].pos - h->entries[i].pos;
  for (uint64_t k = 1; k < period; k++)
    if (h->entries[j + k].pos - h->entries[i + k].pos != shift)
      return 0;
  *shift_out = shift;
  return 1;
}

uint8_t history_entry_bit(const HistoryEntry *e, int64_t pos) {
  int64_t offset = pos - e->tape_origin;
  if (offset < 0)
    return 0;
  uint64_t bit = (uint64_t)offset;
  uint64_t byte = bit / 8;
  if (byte >= e->tape_size)
    return 0;
  return (uint8_t)((e->tape_data[byte] >> (bit % 8)) & 1U);
}

int translated_half_equal(const HistoryEntry *a, const HistoryEntry *b,
                          int64_t shift, int64_t boundary, int dir) {
  if (a->min_pos > a->max_pos || b->min_pos > b->max_pos)
    return 0;
  if (a->state != b->state || b->pos - a->pos != shift)
    return 0;

  int64_t start, end;
  if (dir > 0) {
    // right half
    int64_t b_end = b->max_pos - shift;
    end = a->max_pos > b_end ? a->max_pos : b_end;
    start = boundary;
    if (end < start)
      return 1;
  } else {
    // left half
    int64_t b_start = b->min_pos - shift;
    start = a->min_pos < b_start ? a->min_pos : b_start;
    end = boundary;
    if (start > end)
      return 1;
  }

  for (int64_t pos = start; pos <= end; pos++)
    if (history_entry_bit(a, pos) != history_entry_bit(b, pos + shift))
      return 0;
  return 1;
}

int translated_configs_equal(const HistoryEntry *a, const HistoryEntry *b,
                             int64_t shift) {
  int64_t lo = a->min_pos, hi = a->max_pos;
  int64_t b_lo = b->min_pos - shift, b_hi = b->max_pos - shift;
  if (b_lo < lo)
    lo = b_lo;
  if (b_hi > hi)
    hi = b_hi;
  if (a->state != b->state || b->pos - a->pos != shift)
    return 0;
  for (int64_t pos = lo; pos <= hi; pos++)
    if (history_entry_bit(a, pos) != history_entry_bit(b, pos + shift))
      return 0;
  return 1;
}

int64_t block_head_extremum(const History *h, uint64_t i, uint64_t period,
                            int want_min) {
  int64_t ext = h->entries[i].pos;
  for (uint64_t k = 1; k < period; k++) {
    int64_t pos = h->entries[i + k].pos;
    if (want_min ? pos < ext : pos > ext)
      ext = pos;
  }
  return ext;
}

int block_contains_halt(const History *h, uint64_t i, uint64_t period) {
  for (uint64_t k = 0; k < period; k++)
    if (h->entries[i + k].state == 0)
      return 1;
  return 0;
}

///// History

History history_create(uint64_t initial_cap) {
  if (initial_cap == 0)
    initial_cap = 1024;
  History h;
  h.entries = malloc(initial_cap * sizeof(HistoryEntry));
  if (!h.entries)
    exit(ENOMEM);
  h.len = 0;
  h.cap = initial_cap;
  return h;
}

void history_free(History *h) {
  if (!h)
    return;
  for (uint64_t i = 0; i < h->len; i++)
    free(h->entries[i].tape_data);
  free(h->entries);
}

void history_append(History *h, uint8_t state, const Tape *tape) {
  if (!h)
    return;
  if (h->len == h->cap) {
    h->cap *= 2;
    h->entries = realloc(h->entries, h->cap * sizeof(HistoryEntry));
    if (!h->entries)
      exit(ENOMEM);
  }
  HistoryEntry tmp = {
      .tape_data = tape->data,
      .tape_size = tape->size,
      .tape_origin = tape->origin,
  };
  int64_t head = tape->origin + (int64_t)(8 * tape->up) + (int64_t)tape->lp;
  HistoryEntry *e = &h->entries[h->len++];
  *e = (HistoryEntry){
      .state = state,
      .read = history_entry_bit(&tmp, head),
      .pos = head,
      .min_pos = tape->min_pos,
      .max_pos = tape->max_pos,
      .tape_data = malloc(tape->size),
      .tape_size = tape->size,
      .tape_origin = tape->origin,
  };
  if (!e->tape_data)
    exit(ENOMEM);
  memcpy(e->tape_data, tape->data, tape->size);
}

///// Main loop detector

LoopType history_detect_loop(const History *h, uint64_t max_period) {
  if (!h || h->len < 2)
    return LOOP_TYPE_NONE;

  uint64_t n = h->len;
  uint64_t lim = max_period < n / 2 ? max_period : n / 2;

  for (uint64_t period = 1; period <= lim; period++) {
    uint64_t i = n - 2 * period, j = n - period;

    if (block_contains_halt(h, i, period) || block_contains_halt(h, j, period))
      continue;
    if (!same_transcript_block(h, i, j, period))
      continue;

    int64_t shift = 0;
    if (!constant_shift_block(h, i, j, period, &shift))
      continue;

    if (shift == 0) {
      if (translated_configs_equal(&h->entries[i], &h->entries[j], 0))
        return LOOP_TYPE_EXACT;
      continue;
    }

    int positive = shift > 0;
    int64_t boundary = block_head_extremum(h, i, period, positive);

    if (translated_configs_equal(&h->entries[i], &h->entries[j], shift))
      return positive ? LOOP_TYPE_DRIFT_POSITIVE : LOOP_TYPE_DRIFT_NEGATIVE;

    if (translated_half_equal(&h->entries[i], &h->entries[j], shift, boundary,
                              positive ? 1 : -1))
      return positive ? LOOP_TYPE_DRIFT_POSITIVE : LOOP_TYPE_DRIFT_NEGATIVE;
  }
  return LOOP_TYPE_NONE;
}
