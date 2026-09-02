/* test_fragmentation: create a fragmented heap of mixed sizes, punch holes,
 * refill with different sizes, and confirm nothing overlaps or corrupts. */
#include "test_utils.h"

int main(void) {
  heading("test_fragmentation");
  rng_seed(0xF00D);

  enum { N = 800 };
  unsigned char *p[N];
  size_t sz[N];
  unsigned sd[N];

  for (int i = 0; i < N; i++) {
    sz[i] = (size_t)(rng() % 300) + 1;
    p[i] = malloc(sz[i]);
    sd[i] = rng();
    if (p[i]) fill_pattern(p[i], sz[i], sd[i]);
  }

  /* Free a pseudo-random third of the blocks. */
  for (int i = 0; i < N; i++) {
    if ((rng() % 3) == 0 && p[i]) {
      free(p[i]);
      p[i] = NULL;
    }
  }

  /* Refill the holes with new random sizes. */
  for (int i = 0; i < N; i++) {
    if (p[i] == NULL) {
      sz[i] = (size_t)(rng() % 300) + 1;
      p[i] = malloc(sz[i]);
      sd[i] = rng();
      if (p[i]) fill_pattern(p[i], sz[i], sd[i]);
    }
  }

  int intact = 1;
  for (int i = 0; i < N; i++) {
    if (p[i] && !check_pattern(p[i], sz[i], sd[i])) intact = 0;
  }
  CHECK(intact, "no corruption after fragment/refill cycle");
  CHECK(ts_verify_tags(), "boundary tags consistent");

  for (int i = 0; i < N; i++) free(p[i]);
  dump_freelist("after fragmentation test");
  return test_report("test_fragmentation");
}
