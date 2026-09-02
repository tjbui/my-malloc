/* test_realloc_zero: realloc(p, 0) must free p and return NULL. */
#include "test_utils.h"

int main(void) {
  heading("test_realloc_zero");

  void *p = malloc(64);
  CHECK(p != NULL, "initial malloc(64) succeeded");

  void *r = realloc(p, 0);
  CHECK(r == NULL, "realloc(p, 0) returns NULL");

  /* The heap should still be usable afterwards. */
  void *q = malloc(64);
  CHECK(q != NULL, "malloc still works after realloc(p, 0)");
  free(q);

  dump_freelist("after realloc(p,0) test");
  return test_report("test_realloc_zero");
}
