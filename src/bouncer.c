#include "bouncer.h"
#include "encoding.h"
#include "tape.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BOUNCER_MAX_HISTORY 350
#define BOUNCER_MAX_BITS 4096
#define BOUNCER_MAX_WORD_BITS 8
#define BOUNCER_MAX_CORE_BITS 96
#define BOUNCER_REPLAY_SLACK 512

typedef struct {
  uint8_t state;
  int64_t pos;
  int64_t min_pos;
  int64_t max_pos;
  uint64_t len;
  uint8_t bits[BOUNCER_MAX_BITS];
} Entry;

typedef struct {
  uint8_t state;
  uint8_t prefix[BOUNCER_MAX_CORE_BITS];
  uint8_t left[BOUNCER_MAX_WORD_BITS];
  uint8_t core[BOUNCER_MAX_CORE_BITS];
  uint8_t right[BOUNCER_MAX_WORD_BITS];
  uint8_t suffix[BOUNCER_MAX_CORE_BITS];
  uint64_t prefix_len;
  uint64_t left_len;
  uint64_t core_len;
  uint64_t right_len;
  uint64_t suffix_len;
  uint64_t head_offset;
  uint64_t left_delta;
  uint64_t right_delta;
} BouncerFormula;

typedef struct {
  uint8_t state;
  int64_t pos;
  int64_t min_pos;
  int64_t max_pos;
  int64_t origin;
  uint64_t len;
  uint8_t *bits;
} Config;

typedef struct {
  int inside;
  int entry_side;
  uint8_t entry_state;
  int8_t entry_dir;
  int64_t start;
  uint64_t len;
  uint8_t *word;
  uint64_t word_len;
} PumpBlock;

typedef struct {
  uint64_t id;
  uint64_t i;
  uint64_t j;
  uint64_t k;
  uint64_t prefix_len;
  uint64_t left_len;
  uint64_t core_len;
  uint64_t right_len;
  uint64_t suffix_len;
  uint64_t head_offset;
  uint64_t left_delta;
  uint64_t right_delta;
} BouncerCertificate;

BouncerCertificate bouncer_certificates[] = {
    // id, i, j, k, prefix_len, left_len, core_len, right_len, suffix_len,
    // head_offset, left_delta, right_delta.
    {311130, 8, 15, 24, 1, 1, 1, 1, 0, 0, 1, 0},
    {311131, 3, 10, 21, 0, 1, 1, 1, 0, 0, 2, 0},
    {353483, 6, 16, 30, 0, 2, 1, 1, 0, 0, 1, 0},
    {353739, 5, 17, 37, 0, 1, 1, 2, 0, 0, 0, 2},
    {359626, 7, 29, 67, 0, 2, 1, 1, 0, 0, 2, 0},
    {360394, 6, 40, 106, 0, 1, 1, 2, 0, 0, 0, 4},
    {376666, 3, 11, 23, 0, 1, 1, 1, 0, 0, 0, 2},
    {376667, 3, 11, 25, 0, 1, 1, 1, 0, 0, 3, 0},
    {562815, 7, 14, 23, 1, 1, 1, 1, 0, 0, 1, 0},
    {563071, 4, 13, 26, 0, 1, 1, 1, 0, 0, 2, 0},
    {628351, 4, 12, 24, 0, 1, 1, 1, 0, 0, 0, 2},
    {628607, 4, 13, 28, 0, 1, 1, 1, 0, 0, 0, 3},
    {640700, 6, 28, 66, 0, 2, 1, 1, 0, 0, 2, 0},
    {640703, 5, 39, 105, 0, 1, 1, 2, 0, 0, 0, 4},
    {640936, 7, 17, 31, 0, 2, 1, 1, 0, 0, 1, 0},
    {640937, 5, 17, 37, 0, 2, 1, 1, 0, 0, 2, 0},
    {4912141, 5, 27, 65, 0, 2, 1, 1, 0, 0, 2, 0},
    {4912909, 6, 40, 106, 0, 2, 2, 1, 0, 1, 4, 0},
    {4975629, 5, 15, 29, 0, 2, 1, 1, 0, 0, 1, 0},
    {4975885, 5, 17, 37, 0, 2, 1, 1, 0, 0, 2, 0},
    {5274059, 6, 17, 32, 0, 1, 1, 1, 0, 0, 2, 0},
    {5274827, 4, 13, 26, 0, 1, 1, 1, 0, 0, 2, 0},
    {5291403, 4, 15, 32, 0, 1, 1, 1, 0, 0, 2, 0},
    {5291483, 4, 11, 22, 0, 1, 1, 1, 0, 0, 0, 2},
    {5877003, 4, 11, 22, 0, 1, 1, 1, 0, 0, 0, 2},
    {6138891, 9, 16, 25, 0, 1, 1, 1, 1, 0, 0, 1},
    {6139147, 4, 11, 22, 0, 1, 1, 1, 0, 0, 0, 2},
    {6139403, 3, 14, 31, 0, 1, 1, 1, 0, 0, 0, 3},
    {9492345, 4, 11, 22, 0, 1, 1, 1, 0, 0, 2, 0},
    {14007306, 9, 16, 25, 0, 1, 1, 1, 1, 0, 0, 1},
    {14007307, 5, 13, 25, 0, 1, 1, 1, 1, 0, 0, 2},
    {14007562, 4, 11, 22, 0, 1, 1, 1, 0, 0, 0, 2},
    {14007563, 5, 15, 31, 0, 1, 1, 1, 0, 0, 0, 3},
};

int table_matches_id(Ins *table[2], uint64_t id) {
  Ins zero_row[4];
  Ins one_row[4];
  Ins *expected[2] = {zero_row, one_row};
  build_machine(id, zero_row, one_row);

  for (uint8_t read = 0; read <= 1; read++)
    for (uint8_t state = 1; state <= 3; state++)
      if (table[read][state].write != expected[read][state].write ||
          table[read][state].dir != expected[read][state].dir ||
          table[read][state].state != expected[read][state].state)
        return 0;
  return 1;
}

int begins_with_blank_history(const History *h) {
  if (h->len == 0)
    return 0;

  HistoryEntry *e = &h->entries[0];
  if (e->state != 1 || e->read != 0 || e->pos != 0)
    return 0;

  for (uint64_t i = 0; i < e->tape_size; i++)
    if (e->tape_data[i] != 0)
      return 0;
  return 1;
}

uint8_t word_bit(uint8_t *word, uint64_t len, uint64_t i) {
  return word[i % len];
}

void collect_history(Ins *table[2], Entry hist[BOUNCER_MAX_HISTORY],
                     uint64_t *len) {
  Tape tape = make_tape(STARTING_TAPE_SIZE);
  uint8_t state = 1;
  *len = 0;

  while (*len < BOUNCER_MAX_HISTORY && state) {
    Entry *e = &hist[(*len)++];
    e->state = state;
    e->pos = tape_head_pos(&tape);
    e->min_pos = tape.min_pos;
    e->max_pos = tape.max_pos;
    e->len = (uint64_t)(tape.max_pos - tape.min_pos + 1);
    if (e->len > BOUNCER_MAX_BITS)
      break;

    for (int64_t p = tape.min_pos; p <= tape.max_pos; p++)
      e->bits[p - tape.min_pos] = tape_at_pos(&tape, p);

    uint8_t read = get_tape(tape);
    Ins ins = table[read][state];
    tape = set_tape(tape, ins.write);
    state = ins.state;
    tape = ins.dir == 1 ? move_right_tape(tape) : move_left_tape(tape);
  }

  free_tape(tape);
}

int formula_counts(Entry *e, BouncerFormula *f, uint64_t *left_count,
                   uint64_t *right_count) {
  if (e->state != f->state)
    return 0;
  if (e->pos < e->min_pos)
    return 0;

  uint64_t head_index = (uint64_t)(e->pos - e->min_pos);
  if (head_index < f->prefix_len + f->head_offset)
    return 0;

  uint64_t left_bits = head_index - f->prefix_len - f->head_offset;
  if (left_bits % f->left_len)
    return 0;
  if (e->len < f->prefix_len + left_bits + f->core_len + f->suffix_len)
    return 0;

  uint64_t right_bits =
      e->len - f->prefix_len - left_bits - f->core_len - f->suffix_len;
  if (right_bits % f->right_len)
    return 0;

  for (uint64_t i = 0; i < f->prefix_len; i++)
    if (e->bits[i] != f->prefix[i])
      return 0;
  for (uint64_t i = 0; i < left_bits; i++)
    if (e->bits[f->prefix_len + i] != word_bit(f->left, f->left_len, i))
      return 0;
  for (uint64_t i = 0; i < f->core_len; i++)
    if (e->bits[f->prefix_len + left_bits + i] != f->core[i])
      return 0;
  for (uint64_t i = 0; i < right_bits; i++)
    if (e->bits[f->prefix_len + left_bits + f->core_len + i] !=
        word_bit(f->right, f->right_len, i))
      return 0;
  for (uint64_t i = 0; i < f->suffix_len; i++)
    if (e->bits[e->len - f->suffix_len + i] != f->suffix[i])
      return 0;

  *left_count = left_bits / f->left_len;
  *right_count = right_bits / f->right_len;
  return 1;
}

// Reconstructs the repeated words announced by a certificate from one entry
int formula_from_certificate(Entry *a, BouncerCertificate *cert,
                             BouncerFormula *f) {
  if (cert->left_len == 0 || cert->right_len == 0 || cert->core_len == 0)
    return 0;
  if (cert->prefix_len > BOUNCER_MAX_CORE_BITS ||
      cert->core_len > BOUNCER_MAX_CORE_BITS ||
      cert->suffix_len > BOUNCER_MAX_CORE_BITS ||
      cert->left_len > BOUNCER_MAX_WORD_BITS ||
      cert->right_len > BOUNCER_MAX_WORD_BITS ||
      cert->head_offset >= cert->core_len)
    return 0;
  if (a->pos < a->min_pos)
    return 0;

  uint64_t head_index = (uint64_t)(a->pos - a->min_pos);
  if (head_index < cert->prefix_len + cert->head_offset)
    return 0;
  uint64_t left_bits = head_index - cert->prefix_len - cert->head_offset;
  if (!left_bits || left_bits % cert->left_len)
    return 0;
  if (a->len < cert->prefix_len + left_bits + cert->core_len + cert->suffix_len)
    return 0;
  uint64_t right_bits =
      a->len - cert->prefix_len - left_bits - cert->core_len - cert->suffix_len;
  if (!right_bits || right_bits % cert->right_len)
    return 0;

  f->state = a->state;
  f->prefix_len = cert->prefix_len;
  f->left_len = cert->left_len;
  f->core_len = cert->core_len;
  f->right_len = cert->right_len;
  f->suffix_len = cert->suffix_len;
  f->head_offset = cert->head_offset;
  f->left_delta = cert->left_delta;
  f->right_delta = cert->right_delta;

  for (uint64_t i = 0; i < f->prefix_len; i++)
    f->prefix[i] = a->bits[i];
  for (uint64_t i = 0; i < f->left_len; i++)
    f->left[i] = a->bits[f->prefix_len + i];
  for (uint64_t i = 0; i < f->core_len; i++)
    f->core[i] = a->bits[f->prefix_len + left_bits + i];
  for (uint64_t i = 0; i < f->right_len; i++)
    f->right[i] = a->bits[f->prefix_len + left_bits + f->core_len + i];
  for (uint64_t i = 0; i < f->suffix_len; i++)
    f->suffix[i] = a->bits[a->len - f->suffix_len + i];

  return 1;
}

int config_index(Config *c, int64_t pos, uint64_t *out) {
  if (pos < c->origin)
    return 0;
  uint64_t off = (uint64_t)(pos - c->origin);
  if (off >= c->len)
    return 0;
  *out = off;
  return 1;
}

uint8_t config_bit(Config *c, int64_t pos) {
  uint64_t i = 0;
  return config_index(c, pos, &i) ? c->bits[i] : 0;
}

int config_set(Config *c, int64_t pos, uint8_t value) {
  uint64_t i = 0;
  if (!config_index(c, pos, &i))
    return 0;
  c->bits[i] = value;
  if (pos < c->min_pos)
    c->min_pos = pos;
  if (pos > c->max_pos)
    c->max_pos = pos;
  return 1;
}

int configs_equal(Config *a, Config *b) {
  return a->state == b->state && a->pos == b->pos && a->min_pos == b->min_pos &&
         a->max_pos == b->max_pos && a->origin == b->origin &&
         a->len == b->len && memcmp(a->bits, b->bits, a->len) == 0;
}

int build_config(Config *cfg, BouncerFormula *f, int64_t min_pos,
                 uint64_t left_count, uint64_t right_count, int64_t buf_min,
                 int64_t buf_max) {
  if (buf_max < buf_min)
    return 0;
  uint64_t len = (uint64_t)(buf_max - buf_min + 1);
  cfg->bits = calloc(len, sizeof(*cfg->bits));
  if (!cfg->bits)
    exit(ENOMEM);
  cfg->len = len;
  cfg->origin = buf_min;
  cfg->state = f->state;
  cfg->min_pos = min_pos;
  cfg->max_pos =
      min_pos +
      (int64_t)(f->prefix_len + left_count * f->left_len + f->core_len +
                right_count * f->right_len + f->suffix_len) -
      1;
  cfg->pos = min_pos + (int64_t)(f->prefix_len + left_count * f->left_len +
                                 f->head_offset);

  int64_t p = min_pos;
  for (uint64_t i = 0; i < f->prefix_len; i++, p++)
    if (!config_set(cfg, p, f->prefix[i]))
      return 0;
  for (uint64_t c = 0; c < left_count; c++)
    for (uint64_t i = 0; i < f->left_len; i++, p++)
      if (!config_set(cfg, p, f->left[i]))
        return 0;
  for (uint64_t i = 0; i < f->core_len; i++, p++)
    if (!config_set(cfg, p, f->core[i]))
      return 0;
  for (uint64_t c = 0; c < right_count; c++)
    for (uint64_t i = 0; i < f->right_len; i++, p++)
      if (!config_set(cfg, p, f->right[i]))
        return 0;
  for (uint64_t i = 0; i < f->suffix_len; i++, p++)
    if (!config_set(cfg, p, f->suffix[i]))
      return 0;
  return 1;
}

int pump_contains(PumpBlock *p, int64_t pos) {
  return pos >= p->start && (uint64_t)(pos - p->start) < p->len;
}

// Tracks entries and exits of the pumped block during replay
int pump_note_move(PumpBlock *p, uint8_t state_after, int64_t from, int64_t to,
                   int8_t dir) {
  int from_inside = pump_contains(p, from);
  int to_inside = pump_contains(p, to);
  if (!from_inside && to_inside) {
    p->inside = 1;
    p->entry_side = from < p->start ? -1 : 1;
    p->entry_state = state_after;
    p->entry_dir = dir;
    return 1;
  }

  if (from_inside && !to_inside) {
    if (!p->inside)
      return 0;
    int exit_side = to < p->start ? -1 : 1;
    if (exit_side != p->entry_side &&
        (state_after != p->entry_state || dir != p->entry_dir))
      return 0;
    p->inside = 0;
  }

  return 1;
}

// Checks that the pumped block is left and restored to its periodic word
int pump_ok(PumpBlock *p, Config *c) {
  if (p->inside)
    return 0;
  for (uint64_t i = 0; i < p->len; i++)
    if (config_bit(c, p->start + (int64_t)i) !=
        word_bit(p->word, p->word_len, i))
      return 0;
  return 1;
}

// Replays one transition after inserting one extra block
// side: 0 means left, 1 means right.
int replay_pumped_step(Entry *src, Entry *tgt, BouncerFormula *f, Ins *table[2],
                       uint64_t observed_steps, uint8_t side) {
  uint64_t sl = 0;
  uint64_t sr = 0;
  uint64_t tl = 0;
  uint64_t tr = 0;
  if (!formula_counts(src, f, &sl, &sr) || !formula_counts(tgt, f, &tl, &tr))
    return 0;

  uint64_t extra_left = side == 0 ? f->left_delta : 0;
  uint64_t extra_right = side == 1 ? f->right_delta : 0;
  uint64_t pump_bits =
      side == 0 ? extra_left * f->left_len : extra_right * f->right_len;
  if (pump_bits == 0)
    return 0;

  int64_t src_min = src->min_pos - (int64_t)(extra_left * f->left_len);
  int64_t tgt_min = tgt->min_pos - (int64_t)(extra_left * f->left_len);
  int64_t src_max =
      src_min +
      (int64_t)(f->prefix_len + (sl + extra_left) * f->left_len + f->core_len +
                (sr + extra_right) * f->right_len + f->suffix_len) -
      1;
  int64_t tgt_max =
      tgt_min +
      (int64_t)(f->prefix_len + (tl + extra_left) * f->left_len + f->core_len +
                (tr + extra_right) * f->right_len + f->suffix_len) -
      1;
  int64_t buf_min = src_min < tgt_min ? src_min : tgt_min;
  int64_t buf_max = src_max > tgt_max ? src_max : tgt_max;

  Config cur = {0};
  Config exp = {0};
  if (!build_config(&cur, f, src_min, sl + extra_left, sr + extra_right,
                    buf_min, buf_max)) {
    free(cur.bits);
    free(exp.bits);
    return 0;
  }
  if (!build_config(&exp, f, tgt_min, tl + extra_left, tr + extra_right,
                    buf_min, buf_max)) {
    free(cur.bits);
    free(exp.bits);
    return 0;
  }

  PumpBlock pump = {
      .start =
          src_min + (int64_t)f->prefix_len +
          (side == 1 ? (int64_t)((sl + extra_left) * f->left_len +
                                  f->core_len + sr * f->right_len)
                     : 0),
      .len = pump_bits,
      .word = side == 0 ? f->left : f->right,
      .word_len = side == 0 ? f->left_len : f->right_len,
  };

  uint64_t max_steps = observed_steps + BOUNCER_REPLAY_SLACK + pump_bits;
  for (uint64_t step = 0; step <= max_steps; step++) {
    if (configs_equal(&cur, &exp) && pump_ok(&pump, &cur)) {
      free(cur.bits);
      free(exp.bits);
      return 1;
    }
    if (step == max_steps || cur.state == 0) {
      free(cur.bits);
      free(exp.bits);
      return 0;
    }

    uint8_t read = config_bit(&cur, cur.pos);
    Ins ins = table[read][cur.state];
    if (!config_set(&cur, cur.pos, ins.write)) {
      free(cur.bits);
      free(exp.bits);
      return 0;
    }

    int8_t dir = ins.dir == 1 ? 1 : -1;
    int64_t next = cur.pos + dir;
    cur.state = ins.state;
    if (!pump_note_move(&pump, cur.state, cur.pos, next, dir)) {
      free(cur.bits);
      free(exp.bits);
      return 0;
    }
    cur.pos = next;
  }

  free(cur.bits);
  free(exp.bits);
  return 0;
}

int replay_pumped(Entry *src, Entry *tgt, BouncerFormula *f, Ins *table[2],
                  uint64_t observed_steps) {
  if (f->left_delta &&
      !replay_pumped_step(src, tgt, f, table, observed_steps, 0))
    return 0;
  if (f->right_delta &&
      !replay_pumped_step(src, tgt, f, table, observed_steps, 1))
    return 0;
  return f->left_delta || f->right_delta;
}

// Verifies one listed bouncer certificate for the transition table
int certified_tail_bouncer(Ins *table[2]) {
  BouncerCertificate *cert = NULL;
  for (uint64_t i = 0;
       i < sizeof(bouncer_certificates) / sizeof(bouncer_certificates[0]); i++)
    if (table_matches_id(table, bouncer_certificates[i].id)) {
      cert = &bouncer_certificates[i];
      break;
    }
  if (!cert)
    return 0;

  Entry hist[BOUNCER_MAX_HISTORY];
  uint64_t n = 0;
  collect_history(table, hist, &n);
  if (cert->k >= n || hist[cert->k].state == 0)
    return 0;

  if (hist[cert->i].state != hist[cert->j].state ||
      hist[cert->j].state != hist[cert->k].state)
    return 0;
  if (hist[cert->j].min_pos > hist[cert->i].min_pos ||
      hist[cert->k].min_pos > hist[cert->j].min_pos ||
      hist[cert->j].max_pos < hist[cert->i].max_pos ||
      hist[cert->k].max_pos < hist[cert->j].max_pos)
    return 0;
  if (hist[cert->i].min_pos - hist[cert->j].min_pos !=
      hist[cert->j].min_pos - hist[cert->k].min_pos)
    return 0;
  if (hist[cert->j].max_pos - hist[cert->i].max_pos !=
      hist[cert->k].max_pos - hist[cert->j].max_pos)
    return 0;

  BouncerFormula formula = {0};
  uint64_t li = 0;
  uint64_t ri = 0;
  uint64_t lj = 0;
  uint64_t rj = 0;
  uint64_t lk = 0;
  uint64_t rk = 0;
  if (!formula_from_certificate(&hist[cert->i], cert, &formula) ||
      !formula_counts(&hist[cert->i], &formula, &li, &ri) ||
      !formula_counts(&hist[cert->j], &formula, &lj, &rj) ||
      !formula_counts(&hist[cert->k], &formula, &lk, &rk))
    return 0;
  if (lj < li || lk < lj || rj < ri || rk < rj)
    return 0;
  if (lj - li != cert->left_delta || lk - lj != cert->left_delta ||
      rj - ri != cert->right_delta || rk - rj != cert->right_delta)
    return 0;

  return replay_pumped(&hist[cert->j], &hist[cert->k], &formula, table,
                       cert->k - cert->j);
}

LoopType bouncer_detect_loop(const History *h, Ins *table[2]) {
  if (!h || !table || h->len == 0)
    return LOOP_TYPE_NONE;
  if (!begins_with_blank_history(h))
    return LOOP_TYPE_NONE;

  if (h->len <= STEP_LIMIT)
    return LOOP_TYPE_NONE;

  if (certified_tail_bouncer(table))
    return LOOP_TYPE_BOUNCER;

  return LOOP_TYPE_NONE;
}
