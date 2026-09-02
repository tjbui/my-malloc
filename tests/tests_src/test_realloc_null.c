/* test_realloc_null: realloc(NULL, n) must behave exactly like malloc(n). */
#include "test_utils.h"

int main(void) {
  heading("test_realloc_null");

  unsigned char *p = realloc(NULL, 128);
  CHECK(p != NULL, "realloc(NULL, 128) returned non-NULL");
  if (p) {
    fill_pattern(p, 128, 5);
    CHECK(check_pattern(p, 128, 5), "block from realloc(NULL, n) is usable");
    free(p);
  }

  dump_freelist("after realloc(NULL) test");
  return test_report("test_realloc_null");
}
