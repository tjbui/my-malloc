#include <stdio.h>
#include <stdlib.h>

//void *my_malloc(size_t);

int main() {
  int x = 10;
  int *ptr = malloc(sizeof(int));
  *ptr = x;

  printf("*ptr: %d\n", *ptr);
}
