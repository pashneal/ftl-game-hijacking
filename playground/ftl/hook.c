#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "elf.h"
#include <stdbool.h>


const int FTL_LOG_CALL_INDEX = 0;
const int NATIVE_FTL_LOG_INDEX = 1; 
const int WEAPON_CONTROL_INDEX = 2;

const long unsigned int FTL_BASE_OFFSET = 0x400000; 
const int SELF_PID = -1; // Use -1 to refer to the current process
// read man docs or ptrace(2) for more info on 
// what this struct looks like under the hood
static struct user_regs_struct oldregs; 

typedef struct {
  void * start_executable_addr;
  void * end_executable_addr;
  long offset;
} mem;

typedef struct {
  char name[64]; // Null terminated name of the symbol 
  void * addr;   // Address of the symbol in memory
} entry;
static entry memo[10] = {};

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
  long unsigned int *start_addr;
  long unsigned int *end_addr;
  long unsigned int *offset;
  char perms[5];
  char line[850];
  

  while (fgets(line, 850, fp) != NULL) {
    sscanf(line, "%lx-%lx %s %lx %*s %*s %s", &start_addr, &end_addr, perms, &offset, memory_location);

    if (strstr(memory_location, needle) != NULL && strstr(perms, "x") != NULL) {
      puts("----------------Found one!");
      printf("[+] Memory location: %s\n", memory_location);
      printf("[+] Memory permissions: %s\n", perms);
      printf("[+] Memory address starts: %p\n", (void *)start_addr);
      printf("[+] Memory address end: %p\n", (void *)end_addr);
      printf("[+] Memory offset: %p\n", (void *)offset); 
      printf("[+] Memory line: %s", line);
      puts("----------------Found one!");
      fclose(fp);

      result->start_executable_addr = (void *)start_addr;
      result->end_executable_addr = (void *)end_addr;
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

int overwrite_mem( mem * base_offsets, int target_location, void * data, int size) {
  long unsigned int * addr = base_offsets->start_executable_addr - base_offsets->offset;

  // ugly pointer math, but necessary :(
  addr = (long unsigned int *)((long unsigned int)addr + target_location); 

  printf("[+] Overwriting memory at %p, size: %d\n", addr, size);

  int mem_size = base_offsets->end_executable_addr - base_offsets->start_executable_addr;
  void * start_addr = (void *)base_offsets->start_executable_addr;
  if (mprotect(start_addr, mem_size, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
    puts("[!] Couldn't change memory permissions");
    perror("[!] mprotect failed");
    return 1;
  }


  if(memcpy(addr, data, size) == NULL) {
    puts("[!] Couldn't overwrite memory");
    return 1;
  }

  puts("[+] Overwrote mem!");
  return 0;
}

int overwrite_addr(void * addr, void * data, int size) {
  printf("[+] Overwriting memory at %p, size: %d\n", addr, size);
  if(memcpy(addr, data, size) == NULL) {
    puts("[!] Couldn't overwrite memory");
    return 1;
  }
  puts("[+] Overwrote addr!");
  return 0;
}

int overwrite_buffer( int pid, mem * base_offsets, int target_location, void * data, int size) {
  void * addr = base_offsets->start_executable_addr - base_offsets->offset;
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
  void * addr = base_offsets->start_executable_addr - base_offsets->offset;
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

int overwrite_jump_placeholder(void * addr, int index, char * buffer) {
  long unsigned int a = (long unsigned int)addr;
  for (int i = 0; i < 4; i++) {
    buffer[index + i] = (char)((a >> (i * 8)) & 0xFF); // Extract each byte
  }
}

int overwrite_placeholder(void * addr, int index, char * buffer) {
  long unsigned int a = (long unsigned int)addr;
  for (int i = 0; i < 8; i++) {
    buffer[index + i] = (char)((a >> (i * 8)) & 0xFF); // Extract each byte
  }
}

// Make sure that this is little-endian
int addr_to_buffer(mem * offsets, char * buffer, int index, int target_location) {
  void * addr = (void *)(offsets->start_executable_addr - offsets->offset + target_location);
  overwrite_placeholder(addr, index, buffer);
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
  void * addr = base_offsets->start_executable_addr - base_offsets->offset;
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
  long offset_from_exec = (int)(reference_addr - library_offsets->start_executable_addr);
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
    offset -= FTL_BASE_OFFSET;
    printf("[+] offset adjusted: %p\n", (void *)offset);
    return offset;
  } 
  return -1;
}

bool ftl_log_call() { 
  puts("[+] ftl_log_call called! This is a placeholder function.");
  return 1;
}

bool ftl_log_wrapper() { 
  typedef bool (*ftl_log_t)(const char *msg, ...);
  ftl_log_t ftl_log = (ftl_log_t)memo[NATIVE_FTL_LOG_INDEX].addr;
  ftl_log("Denied: %s\n", memo[FTL_LOG_CALL_INDEX].name);
  return 1;
}


void * allocate_near(mem * offsets, int size) {
  // use map fixed
  void * addr = mmap(
      (void *)offsets->end_executable_addr, 
      size, 
      PROT_READ | PROT_WRITE | PROT_EXEC, 
      MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 
      SELF_PID, 
      0
  );
  if (addr == MAP_FAILED) {
    puts("[!] mmap failed");
    perror("[!] mmap failed");
    return NULL;
  }
  printf("[+] mmap succeeded, allocated at %p\n", addr);
  return addr;
}

__attribute__ ((constructor)) int hook() {

  /*==========Goal 3: Stretch Goal 1: convert to library constructor call========
   0) find target function address in memory (cannot fork: must be hardcoded or elf parsed)
   1) try to overwrite some memory location (done)
   2) try to memprotect to change permissions (done)
   3) automatically find addresses based on start and end (done)
   */

  /*char buffer[20] = {  */
    /*0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,  // mov rax, 0x01*/
    /*0xC3, // retn*/
  /*}; */

  /*mem * mem_offsets = malloc(sizeof(mem));*/
  /*long unsigned int target_offset = 0x596920 - FTL_BASE_OFFSET;*/
  /*printf("[+] Target offset: %p\n", (void *)target_offset);*/

  /*if (target_offset == -1) { return 1; }*/
  /*if (search_mem(SELF_PID, "FTL", mem_offsets) != 0) { return 1; }*/
  /*overwrite_mem(mem_offsets, target_offset, (char *)buffer, 8);*/

  /*============Goal 4: Stretch Goal 2: elf.h to spit out target symbol address========*/
  /*long unsigned int target_offset = 0;*/
  /*long unsigned int test_offset = 0;*/
  /*elf_find_symbol("./FTL.amd64", "_Z7ftl_logPKcz", &test_offset);*/
  /*elf_find_symbol("./FTL.amd64", "_ZN13WeaponControl7KeyDownEi", &target_offset);*/

  /*printf("[+] Test offset: %p\n", (void *)test_offset);*/
  /*printf("[+] Target offset: %p\n", (void *)target_offset);*/

  /*char buffer[20] = {  */
    /*0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,  // mov rax, 0x01*/
    /*0xC3, // retn*/
  /*}; */

  /*if (target_offset == 0) { */
    /*puts("[!] Couldn't find target offset");*/
    /*return 1; */
  /*}*/

  /*mem * mem_offsets = malloc(sizeof(mem));*/
  /*target_offset -= FTL_BASE_OFFSET;*/

  /*if (search_mem(SELF_PID, "FTL", mem_offsets) != 0) { return 1; }*/
  /*overwrite_mem(mem_offsets, target_offset, (char *)buffer, 8);*/


  /*============Goal 5a: replace call to target func with call===========
    1) to puts saying "denied" or something
    2) maybe we can write raw c for that?*/

  /*char buffer[40] = { */
    /*// mov rax, <fake address so we can overwrite it later>*/
    /*0x48, 0xB8, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, */
    /*0xFF, 0xE0 // jmp rax*/
  /*}; */
  /*long unsigned int target_offset = 0;*/
  /*long unsigned int log_offset = 0;*/
  /*long unsigned int test_offset = 0;*/
  /*elf_find_symbol("./FTL.amd64", "_Z7ftl_logPKcz", &log_offset);*/
  /*elf_find_symbol("./FTL.amd64", "_ZN13WeaponControl7KeyDownEi", &target_offset);*/
  /*elf_find_symbol("/home/neal/Github/dev/ftl-game-hijacking/playground/ftl/hook.so", "ftl_log_call", &test_offset);*/

  /*mem * hook_offsets = malloc(sizeof(mem));*/
  /*if (search_mem(SELF_PID, "hook.so", hook_offsets) != 0) { return 1; }*/
  /*printf("[+] hook.so start: %p\n", hook_offsets->start_executable_addr);*/

  /*printf("[+] Log offset: %p\n", (void *)log_offset);*/
  /*printf("[+] Target offset: %p\n", (void *)target_offset);*/
  /*printf("[+] Test offset: %p\n", (void *)test_offset);*/
  /*printf("[+] ftl_log_call address: %p\n", ftl_log_call);*/
  /*printf("[+] ftl_log_call address: %p\n", &ftl_log_call);*/

  /*if (log_offset == 0) { */
    /*puts("[!] Couldn't find ftl_log() offset");*/
    /*return 1; */
  /*}*/
  /*if (target_offset == 0) { */
    /*puts("[!] Couldn't find target offset");*/
    /*return 1; */
  /*}*/

  /*mem * mem_offsets = malloc(sizeof(mem));*/
  

  /*if (search_mem(SELF_PID, "FTL", mem_offsets) != 0) { return 1; }*/
  /*overwrite_placeholder(ftl_log_call, 2, buffer); // Overwrite placeholder in mov r11*/

  /*[>overwrite_placeholder((void *)log_offset, 12, buffer);  // Overwrite placeholder in mov rdx<]*/

  /*target_offset -= FTL_BASE_OFFSET;*/
  /*overwrite_mem(mem_offsets, target_offset, (char *)buffer, 12);*/

  /*==========Goal 5b: replace call to target func with ftl_log call===========
   1) saying "denied" or something, 
   2) approach maybe using a jump hash table*/

  /*char buffer[40] = { */
    /*// mov rax, <fake address so we can overwrite it later>*/
    /*0x48, 0xB8, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, */
    /*0xFF, 0xE0 // jmp rax*/
  /*}; */

  /*long unsigned int target_offset = 0;*/
  /*long unsigned int log_offset = 0;*/
  /*elf_find_symbol("./FTL.amd64", "_Z7ftl_logPKcz", &log_offset);*/
  /*elf_find_symbol("./FTL.amd64", "_ZN13WeaponControl7KeyDownEi", &target_offset);*/

  /*memo[FTL_LOG_CALL_INDEX] = (entry) {*/
    /*.name = "ftl_log_wrapper",*/
    /*.addr = (void *)ftl_log_wrapper*/
  /*};*/

  /*memo[NATIVE_FTL_LOG_INDEX] = (entry) {*/
    /*.name = "_Z7ftl_logPKcz",*/
    /*.addr = (void *)log_offset*/
  /*};*/

  /*memo[WEAPON_CONTROL_INDEX] = (entry) {*/
    /*.name = "_ZN13WeaponControl7KeyDownEi",*/
    /*.addr = (void *)target_offset*/
  /*};*/

  /*mem * mem_offsets = malloc(sizeof(mem));*/
  /*if (search_mem(SELF_PID, "FTL", mem_offsets) != 0) { return 1; }*/

  /*overwrite_placeholder(ftl_log_wrapper, 2, buffer); */
  /*target_offset -= FTL_BASE_OFFSET;*/
  /*overwrite_mem(mem_offsets, target_offset, (char *)buffer, 22);*/

  /*=============Goal 6: simple relay function======================
   1) jump to a relay function 
   2) does something extra, return*/

  char relay[40] = { 
    0xE9, 0x00, 0x00, 0x00, 0x00, // jmp <placeholder> 
  }; 
  ; 
  char trampoline[40] = { 
    // mov rax, <fake address so we can overwrite it later>
    0x48, 0xB8, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 
    0xFF, 0xE0 // jmp rax
  }; 

  long unsigned int target_offset = 0;
  long unsigned int log_offset = 0;
  elf_find_symbol("./FTL.amd64", "_Z7ftl_logPKcz", &log_offset);
  elf_find_symbol("./FTL.amd64", "_ZN13WeaponControl7KeyDownEi", &target_offset);

  memo[FTL_LOG_CALL_INDEX] = (entry) {
    .name = "ftl_log_wrapper",
    .addr = (void *)ftl_log_wrapper
  };

  memo[NATIVE_FTL_LOG_INDEX] = (entry) {
    .name = "_Z7ftl_logPKcz",
    .addr = (void *)log_offset
  };

  memo[WEAPON_CONTROL_INDEX] = (entry) {
    .name = "_ZN13WeaponControl7KeyDownEi",
    .addr = (void *)target_offset
  };

  mem * mem_offsets = malloc(sizeof(mem));
  if (search_mem(SELF_PID, "FTL", mem_offsets) != 0) { return 1; }

  void * allocated = allocate_near(mem_offsets, 4096); // Allocate a page near the target memory
                                                       // because the 5 byte jump has a limited range
  overwrite_placeholder(ftl_log_wrapper, 2, trampoline); 
  overwrite_addr(allocated, (void *)trampoline, 12); 

  long unsigned int relay_addr =  (long unsigned int)allocated - (target_offset + 5);
  overwrite_jump_placeholder((void *)relay_addr, 1, relay); 
  printf("[+] Relay address: %p\n", (void *)relay_addr);
  printf("[+] Relay address: %p\n", (void *)*((long unsigned int *)(relay + 1)));

  target_offset -= FTL_BASE_OFFSET;
  overwrite_mem(mem_offsets, target_offset, relay, 5);




  /*=============Goal 7: simple prologue trampoline======================
   1) jump to a relay function that saves necessary registers
   2) does whatever the original function said
   3) does something extra */

  /*Goal 7: ida pro script to spit out start and end of interesting functions*/
  /*Goal 8: Stretch Goal 3: code gen for hook? (could use rust, or something good at metaprogramming)*/

  /*Goal ?: maybe instructions for running each goal*/
  return 0;
}

