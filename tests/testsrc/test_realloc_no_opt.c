#include <stdlib.h>
#include <stdio.h>
#include "testing.h"

int
main(int argc, char **argv)
{
    initialize_test(__FILE__);

    printf("Before any allocation\n");
    tags_print(print_object);

    // Allocate memory
    int *mem1 = (int *) mallocing(32, print_status, false);
    printf("mem1 = malloc(64)\n");
    tags_print(print_object);

    int *mem2 = (int *) mallocing(64, print_status, false);
    printf("mem2 = malloc(32)\n");
    tags_print(print_object);

    // Reallocate memory to a larger size
    int *new_mem = (int *) my_realloc(mem2, 64);
    printf("mem2 = realloc(mem2, 64)\n");
    tags_print(print_object);

    finalize_test();

   // Check if realloc extended in place
    if (new_mem == mem2) {
        printf("\nFAIL: realloc extended in place.\n\n");
    } else {
        printf("\nPASS: realloc allocated a new block.\n");
    }
}
