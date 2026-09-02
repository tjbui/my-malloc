/* test_calloc_overflow: calloc(nmemb, size) must detect nmemb*size overflow
 * and return NULL rather than allocating a too-small buffer. */
#include "test_utils.h"

int main(void) {
  heading("test_calloc_overflow");

  /* volatile hides the constants from the compiler's static size analysis. */
  volatile size_t huge = (size_t)-1;

  void *a = calloc(huge, 2);
  CHECK(a == NULL, "calloc(SIZE_MAX, 2) overflow returns NULL");

  void *b = calloc(huge, huge);
  CHECK(b == NULL, "calloc(SIZE_MAX, SIZE_MAX) overflow returns NULL");

  /* A legitimate calloc still works. */
  void *c = calloc(10, 8);
  CHECK(c != NULL, "a normal calloc(10, 8) still succeeds");
  free(c);

  dump_freelist("after calloc overflow test");
  return test_report("test_calloc_overflow");
}
