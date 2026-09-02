/* test_coalesce_right: freeing a block whose RIGHT neighbour is already free
 * must merge the two into one larger free block.
 *
 * Five equal blocks are allocated. Because this allocator carves each new
 * block from the right of the remaining space, allocation order A0..A4 lays
 * them out high->low in memory, i.e. A4 A3 A2 A1 A0 (left to right). A0 and A4
 * stay allocated as guards so the interior merges cannot spill into the
 * fencepost or the big arena remainder. */
#include "test_utils.h"

int main(void) {
  heading("test_coalesce_right");

  size_t req = 120;
  size_t S = ts_actual_size(req);

  unsigned char *A[5];
  for (int i = 0; i < 5; i++) {
    A[i] = malloc(req);
    if (A[i]) fill_pattern(A[i], req, (unsigned)i + 1);
  }
  CHECK(A[0] && A[1] && A[2] && A[3] && A[4], "five blocks allocated");

  /* A2's right neighbour is A1. Free A1 first, then A2 -> merge right. */
  free(A[1]);
  free(A[2]);

  CHECKF(ts_find_free_size(2 * S),
         "A1+A2 coalesced into one block of size %zu", 2 * S);
  CHECK(check_pattern(A[0], req, 1), "guard A0 intact");
  CHECK(check_pattern(A[3], req, 4), "neighbour A3 intact");
  CHECK(check_pattern(A[4], req, 5), "guard A4 intact");
  CHECK(ts_verify_tags(), "boundary tags consistent after coalesce");

  dump_heap("after coalescing A1+A2 (right)");

  free(A[0]); free(A[3]); free(A[4]);
  return test_report("test_coalesce_right");
}
