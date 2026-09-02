# CLAUDE.md

Guidance for Claude Code (and other AI assistants) working in this repository.

## ⚠️ Commit policy (important)

**Do NOT add a `Co-Authored-By: Claude` trailer (or any Claude/Anthropic
co-author line) to commits.** The repository owner does not want Claude listed
as a contributor. Write normal commit messages with no AI attribution.

## Project overview

A custom `malloc` implementation — a segregated free-list allocator with
boundary tags — that can replace the system allocator via `LD_PRELOAD`.

- `src/myMalloc.c` — the allocator: `malloc`, `free`, `calloc`, `realloc`,
  plus internal helpers for splitting, coalescing, and requesting chunks from
  the OS with `sbrk`.
- `src/myMalloc.h` — the `header` block layout, size/state bit helpers, and
  free-list / boundary-tag globals.
- `libmymalloc.so` — the built shared library used with `LD_PRELOAD`.

### Key design points

- Blocks carry a `size_state` field (size with the allocation state packed into
  the low 3 bits) and a `left_size` field for boundary-tag coalescing.
- Free blocks are kept in `N_LISTS` (59) size-class free lists; a bitmap tracks
  which lists are non-empty to speed up searches.
- Memory is grabbed from the OS in `ARENA_SIZE` chunks, bracketed by fenceposts;
  adjacent chunks are coalesced.

## Building

```bash
make            # build libmymalloc.so
make test       # build and run the full test suite
make clean      # remove build artifacts
```

## Tests

Test sources live in `tests/tests_src/`; each is compiled into its own
executable in `tests/tests_executables/` and linked directly against the
allocator. `tests/testall` runs them all and prints a pass/fail summary. Run an
individual executable directly to see its heap dumps.
