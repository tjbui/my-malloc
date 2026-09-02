/* test_distinct_values: give every block a unique value, then confirm none of
 * them clobbered one another (a stronger, ordered overlap check). */
#include "test_utils.h"

int main(void) {
  heading("test_distinct_values");

  enum { N = 500 };
  long *p[N];
  int ok = 1;

  for (int i = 0; i < N; i++) {
    p[i] = malloc(sizeof(long));
    if (!p[i]) { ok = 0; continue; }
    *p[i] = (long)i * 1000003L + 17L;
  }

  for (int i = 0; i < N; i++) {
    if (p[i] && *p[i] != (long)i * 1000003L + 17L) {
      ok = 0;
      printf("    block %d holds wrong value\n", i);
    }
  }
  CHECK(ok, "all 500 blocks retained their unique value");

  for (int i = 0; i < N; i++) free(p[i]);
  dump_freelist("after distinct-values test");
  return test_report("test_distinct_values");
}
