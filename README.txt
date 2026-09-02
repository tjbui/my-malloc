my_malloc
=========

A custom malloc implementation (segregated free lists + boundary tags) that can
replace the system allocator via LD_PRELOAD.

Build
-----
  $ make            # build libmymalloc.so
  $ make test       # build and run the test suite
  $ make clean      # remove build artifacts

Tests
-----
  Each test in tests/tests_src is compiled into its own executable in
  tests/tests_executables. `tests/testall` runs them all and reports pass/fail.
  Run an individual executable directly to see its heap output.

Notes
-----
Bitmap:
  - A bitmap tracks which free lists currently hold an unallocated block. This
    is faster than scanning freelistSentinels[] directly.

Realloc:
  - realloc() frees the original pointer, allocates a new block, and copies the
    data over with memcpy(), returning the new pointer.

LD_PRELOAD:
  - The allocator can be preloaded so any program uses it instead of the system
    malloc. Verified on simple commands (ls, pwd, echo, touch) and heavier
    programs (curl, w3m fetching a web page).

Usage
-----
  $ gcc -shared -fPIC -o libmymalloc.so src/myMalloc.c -pthread
  $ LD_PRELOAD=./libmymalloc.so /bin/echo hello
  $ LD_PRELOAD=./libmymalloc.so touch hello.txt
  $ LD_PRELOAD=./libmymalloc.so curl -sL google.com
