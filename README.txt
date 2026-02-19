Bitmap Implementation:
  - Use bitmap to track which freelists currently have an unallocated block. This optimizes the runtime because it is faster to iterate through the bitmap than to iterate through the freelistSentinels[].

Realloc:
  - Created test_realloc.c in the tests/testsrc directory. Modified Makefile to compile upon make
  - test_realloc.c allocates two chunks of memory and frees the chunk of memory on the right. Then, it calls realloc()
  - Without optimization, realloc() will free the original pointer and call myMalloc.c to create a new chunk of memory and use memcpy() to copy the original pointer. Thus, without optimization, the pointer after the realloc() call will be DIFFERENT from the pointer before the realloc() call
  - With optimization, if the right chunk is free and big enough to satisfy the realloc() request, memcpy() will not be called and instead, the chunk will extend in place. Thus, with optimizations, the pointer after the realloc() call will be the SAME from the pointer before the realloc() call. 
  - As shown from running the executable /tests/test_realloc, the pointer stays in the same place so the implenentation is correct

LD_PRELOAD
  - I am able to run hello.c with loading my malloc implementation as a shared library 

Usage:
$ gcc -shared -fPIC -o libmymalloc.so myMalloc_sharedlib.c -ldl -pthread
$ LD_PRELOAD=./libmymalloc.so /bin/echo hello
$ LD_PRELOAD=./libmymalloc.so touch hello.txt
