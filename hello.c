#include <stdio.h>

void *malloc(unsigned long size);

int main() {
    printf("Hello, World!\n");
    int *ptr = (int *)malloc(sizeof(int));
    ptr = 5;
    return 0;
}
