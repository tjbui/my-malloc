/* test_realloc_preserve: hammer realloc up and down many times on the same
 * logical buffer and confirm the min-of-old-and-new prefix always survives. */
#include "test_utils.h"

int main(void) {
  heading("test_realloc_preserve");
  rng_seed(0x1234);

  size_t cur = 32;
  unsigned char *p = malloc(cur);
  CHECK(p != NULL, "initial malloc succeeded");
  unsigned seed = 1;
  fill_pattern(p, cur, seed);

  int ok = 1;
  for (int i = 0; i < 400; i++) {
    size_t next = (size_t)(rng() % 8192) + 1;
    unsigned char *np = realloc(p, next);
    if (!np) { ok = 0; break; }
    size_t keep = next < cur ? next : cur;
    if (!check_pattern(np, keep, seed)) { ok = 0; break; }
    p = np;
    cur = next;
    seed++;
    fill_pattern(p, cur, seed);
  }

  CHECK(ok, "prefix preserved across 400 random reallocs");
  free(p);
  dump_freelist("after realloc preserve test");
  return test_report("test_realloc_preserve");
}
