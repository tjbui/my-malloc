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
    int *mem1 = NULL;
    printf("mem1 = NULL\n");
    tags_print(print_object);

    int *new_mem = (int *) my_realloc(mem1, 8);
    printf("new_mem = realloc(mem1, 8)\n");
   
    if (new_mem == NULL) {
      printf("New memory is null: pass\n");
    }
    else {
      printf("New memory is not null: fail");
    }

    new_mem = my_realloc(mem1, -8);

    if (new_mem == NULL) {
      printf("New memory is null: pass\n");
    }
    else {
      printf("New memory is not null: fail");
    }

    finalize_test();

}
