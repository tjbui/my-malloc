/* test_multiple_alloc: many simultaneous allocations must all succeed and be
 * distinct pointers. */
#include "test_utils.h"

int main(void) {
  heading("test_multiple_alloc");

  enum { N = 300 };
  void *p[N];
  int non_null = 1;

  for (int i = 0; i < N; i++) {
    p[i] = malloc(48);
    if (!p[i]) non_null = 0;
  }
  CHECK(non_null, "all 300 allocations succeeded");

  /* No two pointers are equal. */
  int distinct = 1;
  for (int i = 0; i < N && distinct; i++) {
    for (int j = i + 1; j < N; j++) {
      if (p[i] == p[j]) { distinct = 0; break; }
    }
  }
  CHECK(distinct, "all 300 pointers are distinct");

  dump_freelist("with 300 live blocks");

  for (int i = 0; i < N; i++) {
    free(p[i]);
  }
  dump_freelist("after freeing all 300");
  return test_report("test_multiple_alloc");
}
