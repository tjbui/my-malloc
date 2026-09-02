/* test_sequential_free: allocate N blocks, free them in allocation order, and
 * confirm the heap is left clean enough to serve a large request afterwards. */
#include "test_utils.h"

int main(void) {
  heading("test_sequential_free");

  enum { N = 1000 };
  unsigned char *p[N];
  for (int i = 0; i < N; i++) {
    p[i] = malloc(100);
    if (p[i]) fill_pattern(p[i], 100, (unsigned)i + 1);
  }

  int intact = 1;
  for (int i = 0; i < N; i++) {
    if (p[i] && !check_pattern(p[i], 100, (unsigned)i + 1)) intact = 0;
  }
  CHECK(intact, "all blocks intact before freeing");

  for (int i = 0; i < N; i++) free(p[i]); /* forward order */
  CHECK(ts_verify_tags(), "boundary tags consistent after sequential free");

  unsigned char *big = malloc(N * 60);
  CHECK(big != NULL, "large alloc works after sequential free (coalesced)");
  free(big);

  dump_freelist("after sequential-free test");
  return test_report("test_sequential_free");
}
