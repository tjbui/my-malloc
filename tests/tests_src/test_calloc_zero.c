/* test_calloc_zero: calloc must return zero-initialised memory. */
#include "test_utils.h"

int main(void) {
  heading("test_calloc_zero");

  size_t n = 1000;
  unsigned char *c = calloc(n, 4);
  CHECK(c != NULL, "calloc(1000, 4) returned non-NULL");

  int zeroed = 1;
  if (c) {
    for (size_t i = 0; i < n * 4; i++) {
      if (c[i] != 0) { zeroed = 0; break; }
    }
  }
  CHECK(zeroed, "calloc memory is fully zero-initialised");

  /* And it is writable across the whole span. */
  if (c) {
    memset(c, 0x7, n * 4);
    CHECK(c[0] == 0x7 && c[n * 4 - 1] == 0x7, "calloc block writable end to end");
  }

  free(c);
  dump_freelist("after calloc zero test");
  return test_report("test_calloc_zero");
}
