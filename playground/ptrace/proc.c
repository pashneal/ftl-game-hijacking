#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

// read man docs or ptrace(2) for more info on 
// what this struct looks like under the hood
static struct user_regs_struct oldregs; 

typedef struct {
  void * executable_addr;
  long offset;
} mem;

int search_mem(int pid, char * needle, mem * result) {
  FILE * fp;
  char filename[64];

  sprintf(filename, "/proc/%d/maps", pid);
  fp = fopen(filename, "r");
  if (fp == NULL) {
    puts("[!] Couldn't open maps file");
    return NULL;
  }

  puts("[+] Opened maps file successfully!");

  char memory_location[400];
  long unsigned int *addr;
  long unsigned int *offset;
  char perms[5];
  char line[850];
  

  while (fgets(line, 850, fp) != NULL) {
    sscanf(line, "%lx-%*x %s %lx %*s %*s %s", &addr, perms, &offset, memory_location);

    if (strstr(memory_location, needle) != NULL && strstr(perms, "x") != NULL) {
      puts("----------------Found one!");
      printf("[+] Memory location: %s\n", memory_location);
      printf("[+] Memory permissions: %s\n", perms);
      printf("[+] Memory address: %p\n", (void *)addr);
      printf("[+] Memory offset: %p\n", (void *)offset); 
      printf("[+] Memory line: %s", line);
      puts("----------------Found one!");
      fclose(fp);

      result->executable_addr = (void *)addr;
      result->offset = (long unsigned int)offset;

      return 0; 
    }
  }
  fclose(fp);
  puts("[!] Couldn't find memory location");
  return 1;
}

int attach(int pid) {
  if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
    puts("[!] Couldn't attach to process \n");
    return 1;
  }
  puts("[+] Attached to process successfully...");
  puts("[+] Waiting for process to fully stop...");
  sleep(1); // Give the process time to stop
            
  puts("[+] Waited for process to fully stop! Complete!");
  return 0;
}

int save_registers(int pid)  {
  long result;
  if(ptrace(PTRACE_GETREGS, pid, NULL, &oldregs) == -1) {
    puts("[!] Couldn't get registers");
    return 1;
  }
  puts("[+] Saved registers successfully!");
  printf("[+] user regs rip: %lld\n", oldregs.rip);
  return 0;
}

int overwrite(int pid, mem * base_offsets, long unsigned int qword, int target_location) {
  void * addr = base_offsets->executable_addr - base_offsets->offset;
  addr += target_location;
  long result = ptrace(PTRACE_POKEDATA, pid, addr, qword);
  if (result == -1) {
    puts("[!] Couldn't overwrite qword");
    return 1;
  }
  printf("[+] Overwrote qword at %p with %lx\n", addr, qword);
  puts("[+] Overwrote qword successfully!");
  return 0;
}

int detach(int pid) {
  if (ptrace(PTRACE_CONT, pid, NULL, NULL) == -1) {
    puts("[!] Couldn't resume process, use kill -CONT <PID> to manually resume");
    return 1;
  }
  puts("[+] Detached successfully!");
  return 0; 

}

int peek(int pid, mem * base_offsets, int target) {
  void * addr = base_offsets->executable_addr - base_offsets->offset;
  addr += target;
  long result = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);
  if (result == -1) {
    puts("[!] Couldn't peek data");
    return 1;
  }
  printf("[+] Peeked data at %p: %lx\n", addr, result);
  return 0;
}

int main(int argc, char ** argv) {
  if (argc < 2) {
    printf("Usage: %s <pid>\n", argv[0]);
    return 1;
  }
  int pid;
  sscanf(argv[1], "%d", &pid);
  mem * mem_offsets = malloc(sizeof(mem));

  if (attach(pid) != 0) { return 1; }
  if (save_registers(pid) != 0) { return 1; }
  if (search_mem(pid, "timer", mem_offsets) != 0) { return 1; }
  if (overwrite(pid, mem_offsets, 0x89f8458b4805508d, 0x117f) != 0) { return 1; } 
  if (peek(pid, mem_offsets, 0x117f) != 0) { return 1; }
  if (detach(pid) != 0) { return 1; }


  return 0;
}
