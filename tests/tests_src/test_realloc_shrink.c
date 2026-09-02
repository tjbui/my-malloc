/* test_realloc_shrink: shrinking a block must preserve the surviving prefix. */
#include "test_utils.h"

int main(void) {
  heading("test_realloc_shrink");

  unsigned char *p = malloc(4096);
  CHECK(p != NULL, "initial malloc(4096) succeeded");
  fill_pattern(p, 4096, 3);

  size_t sizes[] = {2048, 1000, 256, 64, 16, 1};
  int n = (int)(sizeof(sizes) / sizeof(sizes[0]));
  int ok = 1;

  for (int i = 0; i < n; i++) {
    unsigned char *np = realloc(p, sizes[i]);
    if (!np) { ok = 0; break; }
    if (!check_pattern(np, sizes[i], 3)) {
      ok = 0;
      printf("    prefix lost shrinking to %zu\n", sizes[i]);
      break;
    }
    p = np;
  }

  CHECK(ok, "shrinking realloc preserved the prefix at every step");
  free(p);

  dump_freelist("after realloc shrink test");
  return test_report("test_realloc_shrink");
}
