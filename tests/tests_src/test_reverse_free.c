/* test_reverse_free: same as sequential, but free in reverse allocation order
 * (a different coalescing sequence). */
#include "test_utils.h"

int main(void) {
  heading("test_reverse_free");

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

  for (int i = N - 1; i >= 0; i--) free(p[i]); /* reverse order */
  CHECK(ts_verify_tags(), "boundary tags consistent after reverse free");

  unsigned char *big = malloc(N * 60);
  CHECK(big != NULL, "large alloc works after reverse free (coalesced)");
  free(big);

  dump_freelist("after reverse-free test");
  return test_report("test_reverse_free");
}
