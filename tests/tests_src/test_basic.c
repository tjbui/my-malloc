/* test_basic: a single malloc/write/read/free round-trip. */
#include "test_utils.h"

int main(void) {
  heading("test_basic");

  int *a = malloc(sizeof(int));
  CHECK(a != NULL, "malloc(sizeof(int)) returned non-NULL");
  if (a) {
    *a = 12345;
    CHECK(*a == 12345, "integer written and read back");
  }

  char *s = malloc(32);
  CHECK(s != NULL, "malloc(32) returned non-NULL");
  if (s) {
    strcpy(s, "allocator works");
    CHECK(strcmp(s, "allocator works") == 0, "string round-trip");
  }

  dump_heap("after two allocations");

  free(a);
  free(s);
  CHECK(1, "free of both blocks did not crash");

  dump_heap("after freeing both");
  return test_report("test_basic");
}
