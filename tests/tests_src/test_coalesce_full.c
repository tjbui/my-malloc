/* test_coalesce_full: fragment an arena into many small blocks, free them all,
 * then request a block far larger than any individual one. It can only be
 * satisfied if the freed neighbours coalesce back together. */
#include "test_utils.h"

int main(void) {
  heading("test_coalesce_full");

  enum { M = 3000 };
  void *p[M];
  int all = 1;
  for (int i = 0; i < M; i++) {
    p[i] = malloc(64);
    if (!p[i]) all = 0;
  }
  CHECK(all, "fragmented the heap into 3000 small blocks");

  /* Free in a scrambled order to exercise all coalescing cases. */
  for (int i = 0; i < M; i += 2) free(p[i]);      /* evens */
  for (int i = 1; i < M; i += 2) free(p[i]);      /* odds  */

  CHECK(ts_verify_tags(), "boundary tags consistent after freeing everything");

  size_t combined = (size_t)M * 40; /* bigger than any single freed block */
  unsigned char *big = malloc(combined);
  CHECK(big != NULL, "large request satisfied by coalesced space");
  if (big) {
    memset(big, 0x5a, combined);
    CHECK(big[0] == 0x5a && big[combined - 1] == 0x5a,
          "coalesced block fully usable");
    free(big);
  }

  dump_freelist("after full-coalesce test");
  return test_report("test_coalesce_full");
}
