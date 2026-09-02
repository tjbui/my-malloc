/* test_write_read: data written to a block of many different sizes must read
 * back unchanged. */
#include "test_utils.h"

int main(void) {
  heading("test_write_read");

  size_t sizes[] = {1, 2, 4, 8, 15, 16, 17, 31, 64, 100, 255, 512, 1000, 4096, 65536};
  int nsizes = (int)(sizeof(sizes) / sizeof(sizes[0]));
  int ok = 1;

  for (int i = 0; i < nsizes; i++) {
    unsigned char *p = malloc(sizes[i]);
    if (!p) { ok = 0; continue; }
    fill_pattern(p, sizes[i], (unsigned)(i + 1));
    if (!check_pattern(p, sizes[i], (unsigned)(i + 1))) {
      ok = 0;
      printf("    corruption at size %zu\n", sizes[i]);
    }
    free(p);
  }

  CHECK(ok, "every size wrote and read back intact");
  dump_freelist("after write/read test");
  return test_report("test_write_read");
}
