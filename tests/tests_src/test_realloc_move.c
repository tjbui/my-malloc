/* test_realloc_move: when a block is boxed in by a later allocation, realloc
 * must relocate it and still preserve the data. */
#include "test_utils.h"

int main(void) {
  heading("test_realloc_move");

  unsigned char *a = malloc(128);
  CHECK(a != NULL, "malloc a succeeded");
  fill_pattern(a, 128, 21);

  /* Allocate b immediately after so a cannot simply be extended in place. */
  unsigned char *b = malloc(128);
  CHECK(b != NULL, "malloc b succeeded");
  fill_pattern(b, 128, 22);

  unsigned char *a2 = realloc(a, 4096);
  CHECK(a2 != NULL, "realloc(a, 4096) succeeded");
  CHECK(a2 && check_pattern(a2, 128, 21), "a's data preserved across the move");
  CHECK(check_pattern(b, 128, 22), "b was left untouched by a's realloc");

  free(a2);
  free(b);
  dump_freelist("after realloc move test");
  return test_report("test_realloc_move");
}
