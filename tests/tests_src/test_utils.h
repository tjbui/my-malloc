/*
 * Shared helpers for the myMalloc test programs.
 *
 * Every test in tests/tests_src is compiled into its own executable (see the
 * Makefile) and linked directly against src/myMalloc.c, so malloc/free/calloc/
 * realloc resolve to the custom allocator. This header provides:
 *
 *   - CHECK / CHECKF assertion macros with pass/fail counters
 *   - deterministic fill / verify helpers to catch overlaps and corruption
 *   - a small reproducible PRNG
 *   - heap-inspection helpers built on the allocator's own print functions
 *   - structural helpers that walk the free lists / boundary tags directly
 *
 * Each test's main() ends with `return test_report("name");` which prints a
 * summary and returns 0 (all passed) or 1 (something failed).
 */

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "myMalloc.h"

/* ---- pass/fail bookkeeping ---------------------------------------------- */

static int g_pass __attribute__((unused)) = 0;
static int g_fail __attribute__((unused)) = 0;

#define CHECK(cond, msg)                                                      \
  do {                                                                        \
    if (cond) {                                                               \
      g_pass++;                                                               \
      printf("  [PASS] %s\n", msg);                                           \
    } else {                                                                  \
      g_fail++;                                                               \
      printf("  [FAIL] %s   (%s:%d)\n", msg, __FILE__, __LINE__);             \
    }                                                                         \
  } while (0)

#define CHECKF(cond, ...)                                                     \
  do {                                                                        \
    if (cond) {                                                               \
      g_pass++;                                                               \
      printf("  [PASS] ");                                                    \
      printf(__VA_ARGS__);                                                    \
      printf("\n");                                                           \
    } else {                                                                  \
      g_fail++;                                                               \
      printf("  [FAIL] ");                                                    \
      printf(__VA_ARGS__);                                                    \
      printf("   (%s:%d)\n", __FILE__, __LINE__);                             \
    }                                                                         \
  } while (0)

static inline void heading(const char *name) {
  printf("========================================================\n");
  printf(" TEST: %s\n", name);
  printf("========================================================\n");
}

static inline int test_report(const char *name) {
  printf("\n--------------------------------------------------------\n");
  printf(" %s: %d passed, %d failed\n", name, g_pass, g_fail);
  printf("--------------------------------------------------------\n");
  fflush(stdout);
  return g_fail == 0 ? 0 : 1;
}

/* ---- deterministic patterns --------------------------------------------- */

static inline void fill_pattern(unsigned char *p, size_t n, unsigned seed) {
  for (size_t i = 0; i < n; i++) {
    p[i] = (unsigned char)(seed * 131u + i * 31u + 7u);
  }
}

static inline int check_pattern(const unsigned char *p, size_t n, unsigned seed) {
  for (size_t i = 0; i < n; i++) {
    if (p[i] != (unsigned char)(seed * 131u + i * 31u + 7u)) {
      return 0;
    }
  }
  return 1;
}

/* ---- small reproducible PRNG (xorshift32) ------------------------------- */

static uint32_t rng_state __attribute__((unused)) = 2463534242u;

static inline uint32_t rng(void) {
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

static inline void rng_seed(uint32_t s) { rng_state = s ? s : 1u; }

/* ---- heap inspection (uses the allocator's own printers) ---------------- */

static inline void dump_freelist(const char *label) {
  printf("\n----- free list [%s] -----\n", label);
  freelist_print(basic_print);
  printf("----- end free list -----\n\n");
  fflush(stdout);
}

static inline void dump_heap(const char *label) {
  printf("\n----- heap [%s] -----\n", label);
  printf("free list (by size class, sizes include metadata):\n");
  freelist_print(basic_print);
  printf("boundary tags  (F = fencepost, A = allocated, U = unallocated):\n");
  tags_print(print_status);
  printf("\n----- end heap -----\n\n");
  fflush(stdout);
}

/* ---- structural helpers (walk allocator internals directly) ------------- */

/* Mirror the allocator's size computation so tests can predict block sizes. */
static inline size_t ts_actual_size(size_t raw) {
  size_t aligned = (raw + 7) & ~((size_t)7);
  if (aligned <= ALLOC_HEADER_SIZE) {
    return sizeof(header);
  }
  return aligned + ALLOC_HEADER_SIZE;
}

/* Header for a pointer returned by malloc. */
static inline header *ts_header(void *p) {
  return (header *)((char *)p - ALLOC_HEADER_SIZE);
}

/* Count how many blocks are currently on all the free lists. */
static inline int ts_count_free(void) {
  int n = 0;
  for (int i = 0; i < N_LISTS; i++) {
    header *s = &freelistSentinels[i];
    for (header *c = s->next; c != s; c = c->next) {
      n++;
    }
  }
  return n;
}

/* Is there at least one free block of exactly this total size? */
static inline int ts_find_free_size(size_t sz) {
  for (int i = 0; i < N_LISTS; i++) {
    header *s = &freelistSentinels[i];
    for (header *c = s->next; c != s; c = c->next) {
      if (get_size(c) == sz) {
        return 1;
      }
    }
  }
  return 0;
}

/* Walk every OS chunk's boundary tags and confirm the size fields agree with
 * the neighbouring blocks' left_size fields. Returns 1 if fully consistent. */
static inline int ts_verify_tags(void) {
  for (size_t i = 0; i < numOsChunks; i++) {
    header *chunk = osChunkList[i];
    if (get_state(chunk) != FENCEPOST) {
      return 0;
    }
    for (header *h = get_right_header(chunk); get_state(h) != FENCEPOST;
         h = get_right_header(h)) {
      header *r = get_right_header(h);
      if (r->left_size != get_size(h)) {
        return 0;
      }
    }
  }
  return 1;
}

/* Force the allocator to initialise (harmless tiny alloc) so the structural
 * helpers above are safe to call even before the test's first allocation. */
static inline void ts_touch(void) {
  void *p = malloc(1);
  free(p);
}

#endif /* TEST_UTILS_H */
