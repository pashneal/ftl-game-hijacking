#include <sys/ptrace.h>
#include <stddef.h>
#include <stdio.h>

int main() {
  long result;
  puts("hhello world\n");
  result = ptrace(PTRACE_ATTACH, 449964, NULL, NULL);
  if (result == -1) {
    puts("Couldn't attach to process \n")
  }
  puts("Attached to process successfully!\n");
}
