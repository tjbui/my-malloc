#include <stdio.h>

void *my_malloc(size_t);

int main() {
  int x = 10;
  int *ptr = my_malloc(sizeof(int));
  *ptr = x;

  printf("*ptr: %d\n", *ptr);
}
