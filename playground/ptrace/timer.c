#include <stdio.h>
#include <unistd.h>

int func(int* count) {
  *count = *count + 1;
  printf("Hello there for time #%d\n", *count);
  sleep(1);
}

int func2(int* count) {
  *count = *count + 1;
  printf("[func2] Hello there for time #%d\n", *count);
  sleep(1);
}

int main() {
  printf("[+] Main can be found at 0x%lx\n", (long)main);
  printf("[+] Func can be found at 0x%lx\n", (long)func);

  // Comment out the following line for goals 1-3 to work
  puts("[+] ready to go!");

  int count = 0;
  while (1) {
    func(&count);
  }
}
