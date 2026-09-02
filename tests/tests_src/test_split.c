/* test_split: allocating from a larger free block must split it, leaving a
 * free remainder of exactly (block - request) bytes.
 *
 * Setup: b1 is allocated first (so it sits at the right end, next to the
 * chunk's right fencepost) and an anchor is allocated to its left. Freeing b1
 * therefore leaves an *isolated* free block (both neighbours are non-free), so
 * the next small malloc is forced to carve from it. */
#include "test_utils.h"

int main(void) {
  heading("test_split");

  size_t big_req = 256, small_req = 64;
  size_t S_big = ts_actual_size(big_req);
  size_t S_small = ts_actual_size(small_req);
  size_t remainder = S_big - S_small;

  unsigned char *b1 = malloc(big_req);   /* rightmost block */
  unsigned char *anchor = malloc(64);    /* pins b1 on the left */
  CHECK(b1 && anchor, "setup allocations succeeded");

  free(b1);
  CHECKF(ts_find_free_size(S_big),
         "isolated free block of size %zu exists before split", S_big);
  dump_heap("after freeing b1 (before split)");

  unsigned char *s = malloc(small_req);  /* should split b1 */
  CHECK(s != NULL, "small malloc after free succeeded");

  CHECKF(!ts_find_free_size(S_big),
         "original block of size %zu is gone after split", S_big);
  CHECKF(ts_find_free_size(remainder),
         "split produced a remainder of size %zu", remainder);

  dump_heap("after split");

  free(s);
  free(anchor);
  return test_report("test_split");
}
