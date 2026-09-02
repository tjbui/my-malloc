/* test_alignment: every pointer returned by malloc must be at least
 * 8-byte aligned regardless of the requested size. */
#include "test_utils.h"

int main(void) {
  heading("test_alignment");

  int aligned = 1;
  int non_null = 1;
  void *ptrs[80];

  for (int i = 0; i < 80; i++) {
    size_t req = (size_t)i * 13 + 1; /* 1, 14, 27, ... */
    ptrs[i] = malloc(req);
    if (ptrs[i] == NULL) {
      non_null = 0;
    } else if (((uintptr_t)ptrs[i] & 0x7u) != 0) {
      aligned = 0;
      printf("    size %zu -> misaligned pointer %p\n", req, ptrs[i]);
    }
  }

  CHECK(non_null, "no allocation returned NULL");
  CHECK(aligned, "all returned pointers are 8-byte aligned");

  for (int i = 0; i < 80; i++) {
    free(ptrs[i]);
  }

  dump_freelist("after alignment test");
  return test_report("test_alignment");
}
