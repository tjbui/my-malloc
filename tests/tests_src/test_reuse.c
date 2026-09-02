/* test_reuse: freeing a block and immediately requesting the same size should
 * hand the very same block back (no growth of the heap). */
#include "test_utils.h"

int main(void) {
  heading("test_reuse");

  unsigned char *b1 = malloc(128);   /* rightmost */
  unsigned char *anchor = malloc(64);/* pins b1 on the left */
  CHECK(b1 && anchor, "setup allocations succeeded");

  free(b1);
  dump_freelist("after freeing b1");

  unsigned char *again = malloc(128);
  CHECK(again != NULL, "re-allocation succeeded");
  CHECK(again == b1, "same-size request reused the just-freed block");

  free(again);
  free(anchor);
  dump_freelist("after reuse test");
  return test_report("test_reuse");
}
