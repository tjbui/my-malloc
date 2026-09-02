/* test_boundary_tags: directly validate the boundary-tag invariant - every
 * block's size must equal its right neighbour's left_size, every chunk must be
 * bracketed by fenceposts, and adjacent blocks must tile memory with no gaps or
 * overlaps. */
#include "test_utils.h"

/* Walk one OS chunk and check that blocks tile it exactly between fenceposts. */
static int check_chunk_tiling(header *chunk) {
  if (get_state(chunk) != FENCEPOST) return 0;
  header *h = get_right_header(chunk);
  header *prev = chunk;
  while (get_state(h) != FENCEPOST) {
    if (h->left_size != get_size(prev)) return 0;      /* back-link matches */
    if (get_size(h) == 0) return 0;                    /* no zero-size block */
    prev = h;
    h = get_right_header(h);
  }
  /* h is the right fencepost; its left_size must match the last block. */
  if (h->left_size != get_size(prev)) return 0;
  return 1;
}

int main(void) {
  heading("test_boundary_tags");
  rng_seed(0x7A65);

  /* Build a varied heap: a mix of live and freed blocks of many sizes. */
  enum { N = 600 };
  unsigned char *p[N];
  for (int i = 0; i < N; i++) {
    p[i] = malloc((size_t)(rng() % 400) + 1);
  }
  for (int i = 0; i < N; i++) {
    if (rng() & 1u) { free(p[i]); p[i] = NULL; }
  }

  CHECK(numOsChunks > 0, "at least one OS chunk is registered");

  int tiling_ok = 1;
  for (size_t i = 0; i < numOsChunks; i++) {
    if (!check_chunk_tiling(osChunkList[i])) tiling_ok = 0;
  }
  CHECK(tiling_ok, "every chunk is tiled exactly by its blocks and fenceposts");
  CHECK(ts_verify_tags(), "size / left_size fields agree everywhere");

  dump_heap("mixed live/free heap");

  for (int i = 0; i < N; i++) free(p[i]);
  CHECK(ts_verify_tags(), "tags still consistent after freeing everything");

  return test_report("test_boundary_tags");
}
