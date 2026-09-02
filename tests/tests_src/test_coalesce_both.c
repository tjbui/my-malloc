/* test_coalesce_both: freeing a block whose LEFT and RIGHT neighbours are both
 * free must merge all three into a single block. Layout: A4 A3 A2 A1 A0. */
#include "test_utils.h"

int main(void) {
  heading("test_coalesce_both");

  size_t req = 120;
  size_t S = ts_actual_size(req);

  unsigned char *A[5];
  for (int i = 0; i < 5; i++) {
    A[i] = malloc(req);
    if (A[i]) fill_pattern(A[i], req, (unsigned)i + 1);
  }
  CHECK(A[0] && A[1] && A[2] && A[3] && A[4], "five blocks allocated");

  /* Free both neighbours of A2 (A1 on the right, A3 on the left), then A2. */
  free(A[1]);
  free(A[3]);
  CHECKF(!ts_find_free_size(3 * S),
         "no triple-block yet (only two isolated frees so far)");

  free(A[2]);
  CHECKF(ts_find_free_size(3 * S),
         "A3+A2+A1 coalesced into one block of size %zu", 3 * S);
  CHECK(check_pattern(A[0], req, 1), "guard A0 intact");
  CHECK(check_pattern(A[4], req, 5), "guard A4 intact");
  CHECK(ts_verify_tags(), "boundary tags consistent after coalesce");

  dump_heap("after coalescing A3+A2+A1 (both sides)");

  free(A[0]); free(A[4]);
  return test_report("test_coalesce_both");
}
