/* test_calloc_large: a large calloc that spans multiple OS chunks must still
 * be fully zeroed. */
#include "test_utils.h"

int main(void) {
  heading("test_calloc_large");

  size_t n = 2 * 1024 * 1024; /* 2M elements */
  unsigned char *c = calloc(n, 4); /* 8 MiB, larger than one arena */
  CHECK(c != NULL, "large calloc (8 MiB) returned non-NULL");

  int zeroed = 1;
  if (c) {
    /* sample across the whole span to keep the test fast */
    for (size_t i = 0; i < n * 4; i += 997) {
      if (c[i] != 0) { zeroed = 0; break; }
    }
    /* explicitly check the very last byte */
    if (c[n * 4 - 1] != 0) zeroed = 0;
  }
  CHECK(zeroed, "large calloc block is zeroed throughout");

  free(c);
  dump_freelist("after large calloc test");
  return test_report("test_calloc_large");
}
