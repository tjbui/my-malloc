/* test_malloc_zero: malloc(0) is allowed to return NULL or a unique pointer;
 * either way it must be safe to free and must not alias a real allocation. */
#include "test_utils.h"

int main(void) {
  heading("test_malloc_zero");

  void *p = malloc(0);
  free(p);
  CHECK(1, "malloc(0) followed by free did not crash");

  void *a = malloc(0);
  void *b = malloc(16);
  CHECK(b != NULL, "malloc(16) after malloc(0) succeeded");
  CHECK(a != b, "malloc(0) does not alias a real allocation");

  if (b) {
    memset(b, 0xff, 16);
    CHECK(1, "writing 16 bytes to the real allocation is safe");
  }

  free(a);
  free(b);
  dump_heap("after zero-size test");
  return test_report("test_malloc_zero");
}
