pub const CUSTOM_OPEN: &str = "    // ==== CUSTOM SECTION, EDIT BELOW ====";
pub const CUSTOM_CLOSE: &str = "    // ==== END CUSTOM SECTION, EDIT ABOVE ====";

pub const MAIN_OPEN: &str = "__attribute__((constructor)) int hook() {";
pub const MAIN_CLOSE: &str = "    return 0;\n}";

pub const ALLOCATED_NEAR_MEMORY: &str = 
r#"    mem * mem_offsets = malloc(sizeof(mem));
    if (search_mem(SELF_PID, "FTL", mem_offsets) != 0) { return 1; }
 
    void * allocated = allocate_near(mem_offsets, 4096); // Allocate a page near the target memory"#;

pub const UTILS: &str = 
r#"#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>


const long unsigned int FTL_BASE_OFFSET = 0x400000; 
const int SELF_PID = -1; // Use -1 to refer to the current process

typedef struct {
  void * start_executable_addr;
  void * end_executable_addr;
  long offset;
} mem;

typedef struct {
  char name[64]; // Null terminated name of the symbol 
  void * addr;   // Address of the symbol in memory
} entry;

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
"#;
