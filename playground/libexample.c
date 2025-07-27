#include <stdio.h> 
#include <unistd.h>

void func(int* count) {
  *count = *count + 2;
  printf("Hello there for time #%d\n", *count);
  sleep(1);
}
