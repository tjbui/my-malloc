/* test_exact_fit: when a request exactly fits a free block (no room to split
 * off a whole new header), the entire block is handed out and no tiny
 * remainder is created. */
#include "test_utils.h"

int main(void) {
  heading("test_exact_fit");

  size_t req = 128;
  size_t S = ts_actual_size(req);

  unsigned char *b1 = malloc(req);    /* rightmost */
  unsigned char *anchor = malloc(64); /* pins b1 on the left */
  CHECK(b1 && anchor, "setup allocations succeeded");

  free(b1);
  CHECKF(ts_find_free_size(S), "free block of size %zu present", S);

  /* Request the same size: an exact fit, so the block is used whole. */
  unsigned char *fit = malloc(req);
  CHECK(fit != NULL, "exact-fit allocation succeeded");
  CHECK(fit == b1, "exact-fit reused the block");
  CHECKF(!ts_find_free_size(S),
         "no leftover free block of size %zu after exact fit", S);

  dump_heap("after exact fit");

  free(fit);
  free(anchor);
  return test_report("test_exact_fit");
}
