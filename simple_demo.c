#include <stdio.h>
#include <stdlib.h>

int main() {
  int x = 10;
  int *ptr = malloc(sizeof(int));
  *ptr = x;

  printf("*ptr: %d\n", *ptr);
  free(ptr);
}
