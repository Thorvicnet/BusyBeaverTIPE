#include "beaver.h"
#include "encoding.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint8_t swap_bc_state(uint8_t state) {
  if (state == 2)
    return 3;
  if (state == 3)
    return 2;
  return state;
}

uint64_t transform_id(uint64_t id, uint8_t swap_bc, uint8_t mirror) {
  uint64_t out = 0;
  for (uint8_t out_index = 0; out_index < 6; out_index++) {
    uint8_t in_index = out_index;
    if (swap_bc && out_index >= 2)
      in_index = (uint8_t)(out_index ^ 6U);

    uint8_t code = (uint8_t)((id >> (4U * in_index)) & 0xFU);

    uint8_t write = (uint8_t)(code & 1U);
    uint8_t dir_bit = (uint8_t)(((code >> 1) & 1U) ^ mirror);
    uint8_t next_state = (uint8_t)((code >> 2) & 3U);
    if (swap_bc)
      next_state = swap_bc_state(next_state);

    uint64_t out_code = (uint64_t)(write | (uint8_t)(dir_bit << 1) |
                                   (uint8_t)(next_state << 2));
    out |= out_code << (4U * out_index);
  }
  return out;
}

uint64_t normalize_irrelevant_instructions(uint64_t id) {
  uint8_t code[6];
  for (uint8_t i = 0; i < 6; i++)
    code[i] = (uint8_t)((id >> (4U * i)) & 0xFU);

  for (uint8_t i = 0; i < 6; i++) {
    uint8_t next_state = (uint8_t)((code[i] >> 2) & 3U);
    if (next_state == 0)
      code[i] = 0;
  }

  uint8_t reachable[4] = {0};
  uint8_t changed = 1;
  reachable[1] = 1;
  while (changed) {
    changed = 0;
    for (uint8_t state = 1; state <= 3; state++) {
      if (!reachable[state])
        continue;

      for (uint8_t read = 0; read <= 1; read++) {
        uint8_t index = (uint8_t)((state - 1U) * 2U + read);
        uint8_t next_state = (uint8_t)((code[index] >> 2) & 3U);
        if (next_state != 0 && !reachable[next_state]) {
          reachable[next_state] = 1;
          changed = 1;
        }
      }
    }
  }

  for (uint8_t state = 1; state <= 3; state++) {
    if (reachable[state])
      continue;
    code[(state - 1U) * 2U] = 0;
    code[(state - 1U) * 2U + 1U] = 0;
  }

  uint64_t out = 0;
  for (uint8_t i = 0; i < 6; i++)
    out |= (uint64_t)code[i] << (4U * i);
  return out;
}

uint64_t canonical_id(uint64_t id) {
  uint64_t best = normalize_irrelevant_instructions(id);
  for (uint8_t swap_bc = 0; swap_bc <= 1; swap_bc++) {
    for (uint8_t mirror = 0; mirror <= 1; mirror++) {
      uint64_t candidate =
          normalize_irrelevant_instructions(transform_id(id, swap_bc, mirror));
      if (candidate < best)
        best = candidate;
    }
  }
  return best;
}

int main(void) {
  const uint64_t total = 1ULL << 24; // 16^6 machines
  FILE *log_file = fopen("main.log", "w");
  if (!log_file)
    return EXIT_FAILURE;

  uint64_t halted = 0;
  uint64_t exact_cycle = 0;
  uint64_t positive_drift = 0;
  uint64_t negative_drift = 0;
  uint64_t formula = 0;
  uint64_t bouncer = 0;
  uint64_t unknown = 0;
  uint64_t skipped_symmetric = 0;
  uint64_t classified_unique = 0;
  uint64_t bb3 = 0;

  for (uint64_t id = 0; id < total; id++) {
    uint64_t canon = canonical_id(id);
    if (canon < id) {
      skipped_symmetric++;
      fprintf(log_file, "id=%llu,type=duplicate,ref=%llu\n",
              (unsigned long long)id, (unsigned long long)canon);
      continue;
    }

    Ins zero_row[4];
    Ins one_row[4];
    Ins *table[2] = {zero_row, one_row};
    build_machine(id, zero_row, one_row);
    classified_unique++;

    MachineResult result = run_machine(table);
    switch (result.type) {
    case MACHINE_RESULT_TYPE_HALTED:
      halted++;
      if (bb3 < result.steps)
        bb3 = result.steps;
      fprintf(log_file, "id=%llu,type=halted,ref=%llu\n",
              (unsigned long long)id, (unsigned long long)id);
      break;
    case MACHINE_RESULT_TYPE_NONHALT_LOOP:
      switch (result.loop) {
      case LOOP_TYPE_NONE:
        unknown++;
        fprintf(log_file, "id=%llu,type=unknown,ref=%llu\n",
                (unsigned long long)id, (unsigned long long)id);
        break;
      case LOOP_TYPE_EXACT:
        exact_cycle++;
        fprintf(log_file, "id=%llu,type=exact_cycle,ref=%llu\n",
                (unsigned long long)id, (unsigned long long)id);
        break;
      case LOOP_TYPE_DRIFT_POSITIVE:
        positive_drift++;
        fprintf(log_file, "id=%llu,type=positive_drift,ref=%llu\n",
                (unsigned long long)id, (unsigned long long)id);
        break;
      case LOOP_TYPE_DRIFT_NEGATIVE:
        negative_drift++;
        fprintf(log_file, "id=%llu,type=negative_drift,ref=%llu\n",
                (unsigned long long)id, (unsigned long long)id);
        break;
      case LOOP_TYPE_FORMULA:
        formula++;
        fprintf(log_file, "id=%llu,type=formula,ref=%llu\n",
                (unsigned long long)id, (unsigned long long)id);
        break;
      case LOOP_TYPE_BOUNCER:
        bouncer++;
        fprintf(log_file, "id=%llu,type=bouncer,ref=%llu\n",
                (unsigned long long)id, (unsigned long long)id);
        break;
      }
      break;
    case MACHINE_RESULT_TYPE_UNKNOWN:
      unknown++;
      fprintf(log_file, "id=%llu,type=unknown,ref=%llu\n",
              (unsigned long long)id, (unsigned long long)id);
      break;
    }
  }

  fclose(log_file);

  printf("Total machines: %llu\n", (unsigned long long)total);
  printf("Classified unique: %llu\n", (unsigned long long)classified_unique);
  printf("Skipped symmetric: %llu\n", (unsigned long long)skipped_symmetric);
  printf("Halted: %llu\n", (unsigned long long)halted);
  printf("Exact cycle: %llu\n", (unsigned long long)exact_cycle);
  printf("Positive drift: %llu\n", (unsigned long long)positive_drift);
  printf("Negative drift: %llu\n", (unsigned long long)negative_drift);
  printf("Formula: %llu\n", (unsigned long long)formula);
  printf("Bouncer: %llu\n", (unsigned long long)bouncer);
  printf("Unknown: %llu\n\n", (unsigned long long)unknown);

  printf("\nbb3: %llu\n", (unsigned long long)bb3);

  return EXIT_SUCCESS;
}
