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

const long unsigned int BASE_OFFSET = 0x400000; // offset of FTL 
const int SELF_PID = -1; // Use -1 to refer to the current process
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

  if (pid == SELF_PID) {
    sprintf(filename, "/proc/self/maps");
  } else {
    sprintf(filename, "/proc/%d/maps", pid);
  }

  fp = fopen(filename, "r");
  if (fp == NULL) {
    puts("[!] Couldn't open maps file");
    return 1;
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

// Take the final partial qword and combine it 
// with the original qword, size is the number of bytes to take from the partial
// qword (0 to 7)
long unsigned int combine_qword( long unsigned int original_qword, long unsigned int partial_qword, int size) {
  long unsigned int mask = 0xFFFFFFFFFFFFFFFF; 
  mask <<= size * 8; // Shift mask to the left
  long unsigned int relevant = original_qword &= mask; 
  partial_qword = partial_qword &= ~mask; 
  return partial_qword | relevant; 
}

int overwrite_buffer( int pid, mem * base_offsets, int target_location, void * data, int size) {
  void * addr = base_offsets->executable_addr - base_offsets->offset;
  addr += target_location;

  while (size >= 8) {
    ptrace(PTRACE_POKEDATA, pid, addr, *(long unsigned int *)data);
    data += 8;
    addr += 8;
    size -= 8;
  }

  if (size > 0) {
    long unsigned int partial_qword = *(long unsigned int *)data; 
    long unsigned int original_qword = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);
    long unsigned int final_qword = combine_qword(original_qword, partial_qword, size);
    ptrace(PTRACE_POKEDATA, pid, addr, final_qword);
  }

  puts("[+] Overwrote buffer!");
  return 0;
}

int overwrite_qword( int pid, mem * base_offsets, long unsigned int qword, int target_location) {
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

int overwrite_index(void * addr, int index, char * buffer) {
  long unsigned int a = (long unsigned int)addr;
  for (int i = 0; i < 8; i++) {
    buffer[index + i] = (char)((a >> (i * 8)) & 0xFF); // Extract each byte
  }
}

// Make sure that this is little-endian
int addr_to_buffer(mem * offsets, char * buffer, int index, int target_location) {
  void * addr = (void *)(offsets->executable_addr - offsets->offset + target_location);
  overwrite_index(addr, index, buffer);
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

void * find_symbol(char * symbol)  {
  void * handle = dlopen("./libexample.so", RTLD_NOW);
  if (!handle) {
    fprintf(stderr, "[!] Error opening library: %s\n", dlerror());
    return NULL;
  }
  printf("[+] Opened library successfully: %p\n", handle);
  void * addr = dlsym(handle, symbol);
  if (!addr) {
    fprintf(stderr, "[!] Error finding symbol %s: %s\n", symbol, dlerror());
    dlclose(handle);
    return NULL;
  }

  printf("[+] Found symbol %s at address %p\n", symbol, addr);
  dlclose(handle);
  return addr;
}

int calc_offset(mem * library_offsets, void * reference_addr) {
  long offset_from_exec = (int)(reference_addr - library_offsets->executable_addr);
  printf("[+] Calculated offset from executable: %lx\n", offset_from_exec);
  return offset_from_exec;
}

FILE * readelf(const char * symbol) {
  char * command = "readelf -sX ./FTL.amd64 | grep %s\n";
  char * formatted_command = malloc(strlen(command) + strlen(symbol) + 1);
  sprintf(formatted_command, command, symbol);
  puts("[+] Formatted command:");
  FILE * fp = popen(formatted_command, "r");
  free(formatted_command);
  if (fp == NULL) {
    puts("[!] Error reading command");
    return NULL;
  }
  return fp;
}

unsigned long int get_offset(FILE * command_result) {
  char line[256];
  long unsigned int offset;
  if (fgets(line, sizeof(line), command_result) != NULL) {
    printf("[+] Read line:\n%s", line);
    sscanf(line, "%*d: %lx%*s", &offset);
    printf("[+] offset found: %p\n", (void *)offset);
    offset -= BASE_OFFSET;
    printf("[+] offset adjusted: %p\n", (void *)offset);
    return offset;
  } 
  return -1;
}

int main(int argc, char ** argv) {
  if (argc < 2) {
    printf("Usage: %s <pid>\n", argv[0]);
    return 1;
  }
  int pid;
  sscanf(argv[1], "%d", &pid);
  mem * mem_offsets = malloc(sizeof(mem));
  mem * libexample_offsets = malloc(sizeof(mem));

  /*Goal 1: modify FTL's ArmamentControl fuction*/

  /*char buffer[20] = {  */
    /*0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,  // mov rax, 0x01*/
    /*0xC3, // retn*/
  /*}; */

  /*int target_location = 0x0DE6B0;*/

  /*if (attach(pid) != 0) { return 1; }*/
  /*if (save_registers(pid) != 0) { return 1; }*/
  /*if (search_mem(pid, "FTL", mem_offsets) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }*/
  /*overwrite_buffer(pid, mem_offsets, target_location, (void *)buffer, 8); */
  /*if (peek(pid, mem_offsets, target_location) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }*/
  /*if (detach(pid) != 0) { return 1; }*/

  /*Goal 2: Use readelf plus base offset to grab symbol address and overwrite it*/

  int target_location;
  char * symbol = "_ZN13WeaponControl7KeyDownEi";
  unsigned long int offset;
  FILE * command_result;

  char buffer[20] = {  
    0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,  // mov rax, 0x01
    0xC3, // retn
  }; 

  command_result = readelf(symbol);
  if (command_result == NULL) { return 1; }
  offset = get_offset(command_result);
  if (offset == -1) { return 1; }
  target_location = offset;

  if (attach(pid) != 0) { return 1; }
  if (search_mem(pid, "FTL", mem_offsets) != 0) { return 1; }
  if (peek(pid, mem_offsets, target_location) != 0) { return 1; }
  if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }
  if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }
  overwrite_buffer(pid, mem_offsets, target_location, (void *)buffer, 8); 
  if (peek(pid, mem_offsets, target_location) != 0) { return 1; }
  if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }
  if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }

  /*Goal 3: convert to library constructor call*/
  /*Goal 4: replace call to target func with call to flt_log saying "denied" or something
    maybe we can write raw c for that?*/
  /*Goal 5: simple prologue trampoline, jump to a relay function that saves all registers, does whatever, and restores overwritten bytes*/
  /*Goal 6: ida pro script to spit out start and end of interesting functions*/

  /*Stretch Goal 2: elf.h to spit out target symbol address*/
  /*Stretch Goal 3: do it all in a library constructor
   ptracing is janky but we can decompile and hardcode mem locations instead.
   a cleaner solution down the line would be simply overwriting mem instead of ptracing
   but alas. We have ways of now easily referencing target functions*/

  return 0;
}
