# my_malloc

A custom `malloc` implementation using **segregated free lists** and **boundary
tags**. It can replace the system allocator in any program via `LD_PRELOAD`.

## Build

```bash
make            # build libmymalloc.so
make test       # build and run the test suite
make clean      # remove build artifacts
```

## Usage

```bash
gcc -shared -fPIC -o libmymalloc.so src/myMalloc.c -pthread

LD_PRELOAD=./libmymalloc.so /bin/echo hello
LD_PRELOAD=./libmymalloc.so touch hello.txt
LD_PRELOAD=./libmymalloc.so curl -sL google.com
```

Preloading makes the target program use this allocator instead of the system
`malloc`. Verified on simple commands (`ls`, `pwd`, `echo`, `touch`) and heavier
programs (`curl`, and `w3m` fetching a web page).

## Tests

Each test in `tests/tests_src/` compiles into its own executable in
`tests/tests_executables/`. Run the whole suite with `make test`
(`tests/testall` reports pass/fail), or run an individual executable directly to
see its heap output.

## Implementation notes

- **Bitmap** — a bitmap tracks which free lists currently hold an unallocated
  block, which is faster than scanning `freelistSentinels[]` directly.
- **Realloc** — frees the original pointer, allocates a new block, copies the
  data with `memcpy()`, and returns the new pointer.

## Note

This project is not affiliated with or endorsed by Purdue University. The source
code here is my own, with major changes to the baseline code from the original
project, which is why it is shared in a public repository.
