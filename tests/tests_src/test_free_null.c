/* test_free_null: free(NULL) must be a harmless no-op. */
#include "test_utils.h"

int main(void) {
  heading("test_free_null");

  ts_touch(); /* make sure the allocator is initialised first */

  int before = ts_count_free();
  free(NULL);
  free(NULL);
  free(NULL);
  int after = ts_count_free();

  CHECK(1, "repeated free(NULL) did not crash");
  CHECKF(before == after,
         "free list unchanged by free(NULL) (%d before, %d after)",
         before, after);

  /* Normal allocation still works afterwards. */
  void *p = malloc(64);
  CHECK(p != NULL, "malloc still works after free(NULL)");
  free(p);

  dump_freelist("after free(NULL) test");
  return test_report("test_free_null");
}
