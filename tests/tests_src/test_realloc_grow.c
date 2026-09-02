/* test_realloc_grow: growing a block must preserve the existing contents. */
#include "test_utils.h"

int main(void) {
  heading("test_realloc_grow");

  size_t sizes[] = {16, 64, 256, 1024, 4096, 20000};
  int n = (int)(sizeof(sizes) / sizeof(sizes[0]));

  unsigned char *p = malloc(sizes[0]);
  CHECK(p != NULL, "initial malloc succeeded");
  fill_pattern(p, sizes[0], 77);

  int ok = 1;
  size_t prev = sizes[0];
  for (int i = 1; i < n; i++) {
    unsigned char *np = realloc(p, sizes[i]);
    if (!np) { ok = 0; break; }
    if (!check_pattern(np, prev, 77)) {
      ok = 0;
      printf("    data lost growing to %zu\n", sizes[i]);
      break;
    }
    p = np;
    prev = sizes[i];
    /* extend the pattern over the new space so the next grow is checked too */
    fill_pattern(p, prev, 77);
  }

  CHECK(ok, "growing realloc preserved contents at every step");
  free(p);

  dump_freelist("after realloc grow test");
  return test_report("test_realloc_grow");
}
