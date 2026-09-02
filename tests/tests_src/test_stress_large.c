/* test_stress_large: repeatedly allocate and free large blocks of varying
 * sizes, forcing constant OS-chunk growth and coalescing. */
#include "test_utils.h"

int main(void) {
  heading("test_stress_large");
  rng_seed(0xBADF00D);

  enum { SLOTS = 16, OPS = 400 };
  unsigned char *ptr[SLOTS] = {0};
  size_t size[SLOTS] = {0};
  unsigned seed[SLOTS] = {0};

  int corrupted = 0, null_alloc = 0;

  for (int op = 0; op < OPS; op++) {
    int i = (int)(rng() % SLOTS);
    if (ptr[i] == NULL) {
      size_t n = (size_t)(rng() % (2 * 1024 * 1024)) + 1024; /* up to ~2 MiB */
      ptr[i] = malloc(n);
      if (!ptr[i]) { null_alloc++; continue; }
      size[i] = n; seed[i] = rng();
      /* only stamp a prefix + suffix to keep the test fast */
      fill_pattern(ptr[i], 256, seed[i]);
      ptr[i][n - 1] = (unsigned char)seed[i];
    } else {
      if (!check_pattern(ptr[i], 256, seed[i]) ||
          ptr[i][size[i] - 1] != (unsigned char)seed[i]) {
        corrupted++;
      }
      free(ptr[i]);
      ptr[i] = NULL;
    }
  }

  for (int i = 0; i < SLOTS; i++) {
    if (ptr[i]) {
      if (!check_pattern(ptr[i], 256, seed[i])) corrupted++;
      free(ptr[i]);
    }
  }

  CHECKF(corrupted == 0, "no corruption over %d large ops (%d)", OPS, corrupted);
  CHECKF(null_alloc == 0, "no unexpected NULL returns (%d)", null_alloc);
  CHECK(ts_verify_tags(), "boundary tags consistent after large-stress");

  dump_freelist("after large stress");
  return test_report("test_stress_large");
}
