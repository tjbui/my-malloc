/* test_stress_random: long randomized sequence of malloc/free/realloc with
 * per-block checksum verification. This is the test most likely to catch
 * free-list corruption (which historically showed up as a hang or a crash). */
#include "test_utils.h"

int main(void) {
  heading("test_stress_random");
  rng_seed(0xC0FFEE);

  enum { SLOTS = 512, OPS = 200000 };
  unsigned char *ptr[SLOTS] = {0};
  size_t size[SLOTS] = {0};
  unsigned seed[SLOTS] = {0};

  int corrupted = 0, null_alloc = 0;

  for (int op = 0; op < OPS; op++) {
    int i = (int)(rng() % SLOTS);

    if (ptr[i] == NULL) {
      size_t n = (size_t)(rng() % 4096) + 1;
      ptr[i] = malloc(n);
      if (!ptr[i]) { null_alloc++; continue; }
      size[i] = n; seed[i] = rng();
      fill_pattern(ptr[i], n, seed[i]);
    } else if (rng() & 1u) {
      if (!check_pattern(ptr[i], size[i], seed[i])) corrupted++;
      free(ptr[i]);
      ptr[i] = NULL;
    } else {
      if (!check_pattern(ptr[i], size[i], seed[i])) corrupted++;
      size_t n = (size_t)(rng() % 4096) + 1;
      unsigned char *np = realloc(ptr[i], n);
      if (!np) { null_alloc++; continue; }
      size_t keep = n < size[i] ? n : size[i];
      if (!check_pattern(np, keep, seed[i])) corrupted++;
      ptr[i] = np; size[i] = n; seed[i] = rng();
      fill_pattern(ptr[i], n, seed[i]);
    }
  }

  for (int i = 0; i < SLOTS; i++) {
    if (ptr[i]) {
      if (!check_pattern(ptr[i], size[i], seed[i])) corrupted++;
      free(ptr[i]);
    }
  }

  CHECKF(corrupted == 0, "no corruption over %d ops (%d detected)", OPS, corrupted);
  CHECKF(null_alloc == 0, "no unexpected NULL returns (%d seen)", null_alloc);
  CHECK(ts_verify_tags(), "boundary tags consistent after stress");

  dump_freelist("after random stress");
  return test_report("test_stress_random");
}
