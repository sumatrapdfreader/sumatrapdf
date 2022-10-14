/*
 * Benchmarks for hb_set_t operations.
 */
#include "benchmark/benchmark.h"

#include <cassert>
#include <cstdlib>
#include "hb.h"

void RandomSet(unsigned size, unsigned max_value, hb_set_t* out) {
  hb_set_clear(out);

  srand(size * max_value);
  for (unsigned i = 0; i < size; i++) {
    while (true) {
      unsigned next = rand() % max_value;
      if (hb_set_has (out, next)) continue;

      hb_set_add(out, next);
      break;
    }
  }
}

// TODO(garretrieger): benchmark union/subtract/intersection etc.

/* Insert a 1000 values into set of varying sizes. */
static void BM_SetInsert_1000(benchmark::State& state) {
  unsigned set_size = state.range(0);
  unsigned max_value = state.range(0) * state.range(1);

  hb_set_t* original = hb_set_create ();
  RandomSet(set_size, max_value, original);
  assert(hb_set_get_population(original) == set_size);

  for (auto _ : state) {
    state.PauseTiming ();
    hb_set_t* data = hb_set_copy(original);
    state.ResumeTiming ();
    for (int i = 0; i < 1000; i++) {
      hb_set_add(data, i * 2654435761u % max_value);
    }
    hb_set_destroy(data);
  }

  hb_set_destroy(original);
}
BENCHMARK(BM_SetInsert_1000)
    ->Unit(benchmark::kMicrosecond)
    ->Ranges(
        {{1 << 10, 1 << 16}, // Set Size
         {2, 512}});          // Density

/* Insert a 1000 values into set of varying sizes. */
static void BM_SetOrderedInsert_1000(benchmark::State& state) {
  unsigned set_size = state.range(0);
  unsigned max_value = state.range(0) * state.range(1);

  hb_set_t* original = hb_set_create ();
  RandomSet(set_size, max_value, original);
  assert(hb_set_get_population(original) == set_size);

  for (auto _ : state) {
    state.PauseTiming ();
    hb_set_t* data = hb_set_copy(original);
    state.ResumeTiming ();
    for (int i = 0; i < 1000; i++) {
      hb_set_add(data, i);
    }
    hb_set_destroy(data);
  }

  hb_set_destroy(original);
}
BENCHMARK(BM_SetOrderedInsert_1000)
    ->Unit(benchmark::kMicrosecond)
    ->Ranges(
        {{1 << 10, 1 << 16}, // Set Size
         {2, 512}});          // Density

/* Single value lookup on sets of various sizes. */
static void BM_SetLookup(benchmark::State& state, unsigned interval) {
  unsigned set_size = state.range(0);
  unsigned max_value = state.range(0) * state.range(1);

  hb_set_t* original = hb_set_create ();
  RandomSet(set_size, max_value, original);
  assert(hb_set_get_population(original) == set_size);

  auto needle = max_value / 2;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        hb_set_has (original, (needle += interval) % max_value));
  }

  hb_set_destroy(original);
}
BENCHMARK_CAPTURE(BM_SetLookup, ordered, 3)
    ->Ranges(
        {{1 << 10, 1 << 16}, // Set Size
         {2, 512}});          // Density
BENCHMARK_CAPTURE(BM_SetLookup, random, 12345)
    ->Ranges(
        {{1 << 10, 1 << 16}, // Set Size
         {2, 512}});          // Density

/* Full iteration of sets of varying sizes. */
static void BM_SetIteration(benchmark::State& state) {
  unsigned set_size = state.range(0);
  unsigned max_value = state.range(0) * state.range(1);

  hb_set_t* original = hb_set_create ();
  RandomSet(set_size, max_value, original);
  assert(hb_set_get_population(original) == set_size);

  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  for (auto _ : state) {
    hb_set_next (original, &cp);
  }

  hb_set_destroy(original);
}
BENCHMARK(BM_SetIteration)
    ->Ranges(
        {{1 << 10, 1 << 16}, // Set Size
         {2, 512}});          // Density

/* Set copy. */
static void BM_SetCopy(benchmark::State& state) {
  unsigned set_size = state.range(0);
  unsigned max_value = state.range(0) * state.range(1);

  hb_set_t* original = hb_set_create ();
  RandomSet(set_size, max_value, original);
  assert(hb_set_get_population(original) == set_size);

  for (auto _ : state) {
    hb_set_t *s = hb_set_create ();
    hb_set_set (s, original);
    hb_set_destroy (s);
  }

  hb_set_destroy(original);
}
BENCHMARK(BM_SetCopy)
    ->Unit(benchmark::kMicrosecond)
    ->Ranges(
        {{1 << 10, 1 << 16}, // Set Size
         {2, 512}});          // Density

BENCHMARK_MAIN();
