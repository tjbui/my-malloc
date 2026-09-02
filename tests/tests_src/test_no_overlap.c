/* test_no_overlap: distinct live allocations must occupy disjoint memory.
 * Checked two ways: unique fill patterns and explicit address-range math. */
#include "test_utils.h"

int main(void) {
  heading("test_no_overlap");
  rng_seed(0xA11C);

  enum { N = 256 };
  unsigned char *p[N];
  size_t sz[N];

  for (int i = 0; i < N; i++) {
    sz[i] = (size_t)(rng() % 500) + 1;
    p[i] = malloc(sz[i]);
    if (p[i]) fill_pattern(p[i], sz[i], (unsigned)i + 1);
  }

  int intact = 1;
  for (int i = 0; i < N; i++) {
    if (p[i] && !check_pattern(p[i], sz[i], (unsigned)i + 1)) intact = 0;
  }
  CHECK(intact, "no live block's contents were corrupted by another");

  int ranges_ok = 1;
  for (int i = 0; i < N && ranges_ok; i++) {
    if (!p[i]) continue;
    uintptr_t ai = (uintptr_t)p[i], aend = ai + sz[i];
    for (int j = i + 1; j < N; j++) {
      if (!p[j]) continue;
      uintptr_t bi = (uintptr_t)p[j], bend = bi + sz[j];
      if (ai < bend && bi < aend) { ranges_ok = 0; break; }
    }
  }
  CHECK(ranges_ok, "no two allocated address ranges intersect");

  for (int i = 0; i < N; i++) free(p[i]);
  dump_freelist("after overlap test");
  return test_report("test_no_overlap");
}
