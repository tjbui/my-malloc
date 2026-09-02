/* test_stress_small: churn through a very large number of tiny allocations,
 * which stresses the small size-class free lists and the split path. */
#include "test_utils.h"

int main(void) {
  heading("test_stress_small");
  rng_seed(0x5A5A);

  enum { SLOTS = 4096, OPS = 300000 };
  unsigned char **ptr = calloc(SLOTS, sizeof(*ptr));
  unsigned char *sd = calloc(SLOTS, 1);
  unsigned char *live = calloc(SLOTS, 1);
  CHECK(ptr && sd && live, "bookkeeping arrays allocated");

  int corrupted = 0;
  for (int op = 0; op < OPS; op++) {
    int i = (int)(rng() % SLOTS);
    if (!live[i]) {
      size_t n = (size_t)(rng() % 16) + 1; /* 1..16 bytes */
      ptr[i] = malloc(n);
      if (ptr[i]) {
        sd[i] = (unsigned char)rng();
        ptr[i][0] = sd[i];
        live[i] = 1;
      }
    } else {
      if (ptr[i][0] != sd[i]) corrupted++;
      free(ptr[i]);
      live[i] = 0;
    }
  }

  for (int i = 0; i < SLOTS; i++) {
    if (live[i]) {
      if (ptr[i][0] != sd[i]) corrupted++;
      free(ptr[i]);
    }
  }

  CHECKF(corrupted == 0, "no corruption across %d tiny ops (%d)", OPS, corrupted);
  CHECK(ts_verify_tags(), "boundary tags consistent after small-stress");

  free(ptr); free(sd); free(live);
  dump_freelist("after small stress");
  return test_report("test_stress_small");
}
