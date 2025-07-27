#include <stdio.h>
#include <unistd.h>

int func(int* count) {
  *count = *count + 1;
  printf("Hello there for time #%d\n", *count);
  sleep(1);
}

int main() {
  int count = 0;
  while (1) {
    func(&count);
  }
}
