/* test_large_alloc: a single request larger than one OS arena (ARENA_SIZE
 * defaults to 4 MiB) must succeed and be fully writable. */
#include "test_utils.h"

int main(void) {
  heading("test_large_alloc");

  size_t big = (size_t)10 * 1024 * 1024; /* 10 MiB */
  unsigned char *p = malloc(big);
  CHECK(p != NULL, "10 MiB malloc succeeded");

  if (p) {
    p[0] = 0xA;
    p[big / 2] = 0xB;
    p[big - 1] = 0xC;
    CHECK(p[0] == 0xA && p[big / 2] == 0xB && p[big - 1] == 0xC,
          "first, middle and last bytes all writable");
    /* full memset to be sure the entire span is mapped */
    memset(p, 0xEE, big);
    CHECK(p[big - 1] == 0xEE, "full-span memset succeeded");
    free(p);
  }

  /* A second large request after freeing should also succeed. */
  unsigned char *q = malloc(big);
  CHECK(q != NULL, "second 10 MiB malloc succeeded (space reused)");
  free(q);

  dump_freelist("after large-alloc test");
  return test_report("test_large_alloc");
}
