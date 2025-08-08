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

int override_index(void * addr, int index, char * buffer) {
  long unsigned int a = (long unsigned int)addr;
  for (int i = 0; i < 8; i++) {
    buffer[index + i] = (char)((a >> (i * 8)) & 0xFF); // Extract each byte
  }
}

// Make sure that this is little-endian
int addr_to_buffer(mem * offsets, char * buffer, int index, int target_location) {
  void * addr = (void *)(offsets->executable_addr - offsets->offset + target_location);
  override_index(addr, index, buffer);
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

int main(int argc, char ** argv) {
  if (argc < 2) {
    printf("Usage: %s <pid>\n", argv[0]);
    return 1;
  }
  int pid;
  sscanf(argv[1], "%d", &pid);
  mem * mem_offsets = malloc(sizeof(mem));
  mem * libexample_offsets = malloc(sizeof(mem));

  /* Goal 1: modify the count increase */
  /*if (attach(pid) != 0) { return 1; }*/
  /*if (save_registers(pid) != 0) { return 1; }*/
  /*if (search_mem(pid, "timer", mem_offsets) != 0) { return 1; }*/
  /*if (overwrite_qword(pid, mem_offsets, 0x89f8458b4805508d, 0x117f) != 0) { return 1; } */
  /*if (peek(pid, mem_offsets, 0x117f) != 0) { return 1; }*/
  /*if (detach(pid) != 0) { return 1; }*/

  /* Goal 2a: modify main to call func2 instead of func */
  /*if (attach(pid) != 0) { return 1; }*/
  /*if (save_registers(pid) != 0) { return 1; }*/
  /*if (search_mem(pid, "timer", mem_offsets) != 0) { return 1; }*/
  /*if (overwrite_qword(pid, mem_offsets, 0xf2ebffffff4ee8, 0x125e) != 0) { return 1; } */
  /*if (peek(pid, mem_offsets, 0x125e) != 0) { return 1; }*/
  /*if (detach(pid) != 0) { return 1; }*/

  /* Goal 2b: modify main to call func2 instead of func (with a buffer) */
  /*char buffer[8] = {0xe8, 0x4e, 0xff, 0xff, 0xff, 0xeb, 0xf2, 0x00};*/
  /*if (attach(pid) != 0) { return 1; }*/
  /*if (save_registers(pid) != 0) { return 1; }*/
  /*if (search_mem(pid, "timer", mem_offsets) != 0) { return 1; }*/
  /*overwrite_buffer(pid, mem_offsets, 0x125e, (void *)buffer, 8); */
  /*if (peek(pid, mem_offsets, 0x125e) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, 0x125e + 8) != 0) { return 1; }*/
  /*if (detach(pid) != 0) { return 1; }*/

  /* Goal 3: destructively replace func with func2 call*/
  /*char buffer[5] = {0xe8, 0x48, 0x00, 0x00, 0x00}; // call 0x48 -> call func2*/
  /*if (attach(pid) != 0) { return 1; }*/
  /*if (save_registers(pid) != 0) { return 1; }*/
  /*if (search_mem(pid, "timer", mem_offsets) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, 0x1169) != 0) { return 1; }*/
  /*overwrite_buffer(pid, mem_offsets, 0x1169, (void *)buffer,5); */
  /*if (peek(pid, mem_offsets, 0x1169) != 0) { return 1; }*/
  /*if (detach(pid) != 0) { return 1; }*/

  /* Goal 4a: destructively modify func to call some existing shared library function (puts) */
  /*char buffer[20] = {  */
    /*0x48, 0x8D, 0x05, 0x00, 0x0F, 0x00, 0x00, // lea rax, puts_string_address */
    /*0x48, 0x89, 0xC7, // mov rdi, rax*/
    /*0xE8, 0xD8, 0xFE, 0xFF, 0xFF// call puts*/
  /*}; */

  /*int target_location = 0x1189; // The location of the function call in the code*/

  /*if (attach(pid) != 0) { return 1; }*/
  /*if (save_registers(pid) != 0) { return 1; }*/
  /*if (search_mem(pid, "timer", mem_offsets) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }*/
  /*overwrite_buffer(pid, mem_offsets, target_location, (void *)buffer, 15); */
  /*if (peek(pid, mem_offsets, target_location) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }*/
  /*if (detach(pid) != 0) { return 1; }*/

  /* Goal 4b: figure out what the lea address rule generally is */ 
  // it's just the offset from the next instruction 

  /* Goal 5: modify func to call a function in my shared library (hooking my own) once*/
  // Must do LD_PRELOAD=./libexample.so for this to work

  /*char buffer[20] = { */
    /*// mov rax, <fake address so we can overwrite it later>*/
    /*0x48, 0xB8, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, */
    /*0xFF, 0xE0 // jmp, rax*/
  /*}; */

  /*int target_location = 0x1189; // The location of the function call in the code*/
  /*int library_func_location = 0x1139;*/

  /*if (attach(pid) != 0) { return 1; }*/
  /*if (save_registers(pid) != 0) { return 1; }*/
  /*if (search_mem(pid, "timer", mem_offsets) != 0) { return 1; }*/
  /*if (search_mem(pid, "libexample", libexample_offsets) != 0) { return 1; }*/
  /*addr_to_buffer(libexample_offsets, buffer, 2, library_func_location); */

  /*if (peek(pid, mem_offsets, target_location) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }*/
  /*overwrite_buffer(pid, mem_offsets, target_location, (void *)buffer, 12); */
  /*if (peek(pid, mem_offsets, target_location) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }*/
  /*if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }*/
  /*if (detach(pid) != 0) { return 1; }*/

  /* Goal 6: automatically get the library function address from dlopen/dlsym */
  char buffer[20] = { 
    // mov rax, <fake address so we can overwrite it later>
    0x48, 0xB8, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 
    0xFF, 0xE0 // jmp, rax
  }; 

  int target_location = 0x1189; // The location of "func()"

  // first figure out offsets for the symbol in our own memory
  // Must do LD_PRELOAD=./libexample.so for this to work
  void * library_func_addr = find_symbol("func3");
  if (search_mem(SELF_PID, "libexample", libexample_offsets) != 0) { return 1; }
  int offset = calc_offset(libexample_offsets, library_func_addr);
  if (!library_func_addr) { return 1; }

  if (attach(pid) != 0) { return 1; }
  if (save_registers(pid) != 0) { return 1; }
  if (search_mem(pid, "timer", mem_offsets) != 0) { return 1; }
  if (search_mem(pid, "libexample", libexample_offsets) != 0) { return 1; }

  // apply the same offset to figure out the symbol location in the victim process
  library_func_addr = libexample_offsets->executable_addr + offset;
  override_index(library_func_addr, 2, buffer); 

  if (peek(pid, mem_offsets, target_location) != 0) { return 1; }
  if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }
  if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }
  overwrite_buffer(pid, mem_offsets, target_location, (void *)buffer, 12); 
  if (peek(pid, mem_offsets, target_location) != 0) { return 1; }
  if (peek(pid, mem_offsets, target_location + 8) != 0) { return 1; }
  if (peek(pid, mem_offsets, target_location + 16) != 0) { return 1; }
  if (detach(pid) != 0) { return 1; }

  /* Goal 7: move hooking code to library constructor*/
  /* Goal 8: in shellcode, call hook and return to func (prologue)*/
  /* Goal 9: in shellcode, run func code then call hook (epilogue)*/

  return 0;
}
