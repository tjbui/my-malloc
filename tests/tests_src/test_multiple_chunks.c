/* test_multiple_chunks: allocating far more than one arena's worth of memory
 * forces the allocator to request several chunks from the OS and stitch them
 * together. Verify all the memory is independent and intact. */
#include "test_utils.h"

int main(void) {
  heading("test_multiple_chunks");

  /* ~24 MiB of live data across many blocks => several 4 MiB arenas. */
  enum { N = 48 };
  size_t each = 512 * 1024; /* 512 KiB */
  unsigned char *p[N];
  int all = 1;

  for (int i = 0; i < N; i++) {
    p[i] = malloc(each);
    if (!p[i]) { all = 0; continue; }
    fill_pattern(p[i], each, (unsigned)i + 1);
  }
  CHECK(all, "all 48 large blocks allocated (multiple OS chunks)");
  CHECKF(numOsChunks >= 2, "allocator grabbed multiple OS chunks (%zu)",
         numOsChunks);

  int intact = 1;
  for (int i = 0; i < N; i++) {
    if (p[i] && !check_pattern(p[i], each, (unsigned)i + 1)) intact = 0;
  }
  CHECK(intact, "every block across all chunks retained its data");
  CHECK(ts_verify_tags(), "boundary tags consistent across all chunks");

  for (int i = 0; i < N; i++) free(p[i]);
  dump_freelist("after multiple-chunks test");
  return test_report("test_multiple_chunks");
}
