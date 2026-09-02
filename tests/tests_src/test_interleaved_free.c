/* test_interleaved_free: free every other block first, then the rest. This
 * leaves free "holes" separated by live blocks, then fills them back in,
 * exercising the case-1 (no coalesce) and later coalescing paths. */
#include "test_utils.h"

int main(void) {
  heading("test_interleaved_free");

  enum { N = 1000 };
  unsigned char *p[N];
  for (int i = 0; i < N; i++) {
    p[i] = malloc(80);
    if (p[i]) fill_pattern(p[i], 80, (unsigned)i + 1);
  }

  /* Free the even-indexed blocks: their live odd neighbours prevent merging. */
  for (int i = 0; i < N; i += 2) free(p[i]);
  CHECK(ts_verify_tags(), "tags consistent after freeing the even blocks");

  /* Surviving odd blocks must be untouched. */
  int intact = 1;
  for (int i = 1; i < N; i += 2) {
    if (!check_pattern(p[i], 80, (unsigned)i + 1)) intact = 0;
  }
  CHECK(intact, "odd blocks intact while even holes are open");

  /* Refill the holes. */
  int refilled = 1;
  for (int i = 0; i < N; i += 2) {
    p[i] = malloc(80);
    if (!p[i]) refilled = 0;
    else fill_pattern(p[i], 80, (unsigned)i + 1);
  }
  CHECK(refilled, "reallocated into all the freed holes");

  /* Now free everything. */
  for (int i = 0; i < N; i++) free(p[i]);
  CHECK(ts_verify_tags(), "tags consistent after freeing everything");

  dump_freelist("after interleaved-free test");
  return test_report("test_interleaved_free");
}
