/* test_min_allocation: the smallest requests (1..8 bytes) must still produce
 * usable, non-overlapping blocks that satisfy the allocator's minimum size. */
#include "test_utils.h"

int main(void) {
  heading("test_min_allocation");

  enum { N = 200 };
  unsigned char *p[N];
  int ok = 1;

  for (int i = 0; i < N; i++) {
    size_t req = (size_t)(i % 8) + 1; /* 1..8 */
    p[i] = malloc(req);
    if (!p[i]) { ok = 0; continue; }
    /* At least one usable byte, and the block must be free-able. */
    p[i][0] = (unsigned char)i;
  }
  CHECK(ok, "all tiny allocations (1..8 bytes) succeeded");

  int intact = 1;
  for (int i = 0; i < N; i++) {
    if (p[i] && p[i][0] != (unsigned char)i) intact = 0;
  }
  CHECK(intact, "tiny blocks did not overlap (first byte preserved)");

  /* Every allocation must be at least the minimum block size internally. */
  int min_ok = 1;
  for (int i = 0; i < N; i++) {
    if (p[i] && get_size(ts_header(p[i])) < sizeof(header)) min_ok = 0;
  }
  CHECK(min_ok, "each tiny block is at least the minimum block size");

  for (int i = 0; i < N; i++) free(p[i]);
  dump_freelist("after min-allocation test");
  return test_report("test_min_allocation");
}
