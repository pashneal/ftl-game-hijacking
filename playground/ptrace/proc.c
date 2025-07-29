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

<<<<<<< HEAD
=======
void * search_mem(int pid, char * name) {
  FILE * fp;
  char filename[64];
  char line[850];

  sprintf(filename, "/proc/%d/maps", pid);
  printf("[+] Searching for maps file at: %s\n", filename);

  fp = fopen(filename, "r");
  if (fp == NULL) {
    puts("[!] Couldn't read file from pid");
    return NULL;
  }
  puts("[+] Successfully opened maps file!");

  while (fgets(line, 850, fp) != NULL) {
    long unsigned int * addr;
    char permissions[5];
    char memory_location[400];

    sscanf(line, "%lx-%*x %s %*s %*s %*s %s", addr, permissions, memory_location);

    if (strstr(memory_location, name) != NULL &&
        strstr(permissions, "x") != NULL) {
      puts("[+] >>> Found suitable address: ");
      printf("[+] >>> %p with permissions: %s in location %s\n", (void *)addr, permissions, memory_location);
      fclose(fp);
      return (void *)addr;
    }
  }

  puts("[!] No suitable addresses found in maps file");
  return NULL;
}

>>>>>>> dae69c8 (add files)
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

int detach(int pid) {
  if (ptrace(PTRACE_CONT, pid, NULL, NULL) == -1) {
    puts("[!] Couldn't resume process, use kill -CONT <PID> to manually resume");
    return 1;
  }
  puts("[+] Detached successfully!");
  return 0; 

}

int main() {
<<<<<<< HEAD
  int pid = 450847; 
=======
  int pid = 674334;
>>>>>>> dae69c8 (add files)
  long result;

  if (attach(pid) != 0) { return 1; }
  if (save_registers(pid) != 0) { return 1; }
<<<<<<< HEAD
=======
  if (search_mem(pid, "libexample.so") == NULL) { return 1; }
>>>>>>> dae69c8 (add files)
  if (detach(pid) != 0) { return 1; }

  return 0;
}
