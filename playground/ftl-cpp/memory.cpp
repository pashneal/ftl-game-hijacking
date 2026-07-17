#include "memory.h"
#include <cstring>
#include <iostream>
#include <ios>
#include <ostream>


entry Hook::memo[100];
char** Hook::trampoline_cursor = nullptr;

uintptr_t * SharedMemory::crew_constructor = nullptr;

MemoryOffsets::MemoryOffsets(std::string needle) {
  std::string mapsPath = "/proc/self/maps";
  FILE* mapsFile = fopen(mapsPath.c_str(), "r");

  if (mapsFile == NULL) { return; }

  char memory_location[400];
  long unsigned int start_addr;
  long unsigned int end_addr;
  long unsigned int offset_addr;
  char perms[5];
  char line[1000];

  while (fgets(line, sizeof(line), mapsFile)) {
      sscanf(
          line, 
          "%lx-%lx %s %lx %*s %*s %s", 
          &start_addr, 
          &end_addr, 
          perms, 
          &offset_addr, 
          memory_location
      );

      if (strstr(memory_location, needle.c_str()) != NULL) {
        std::cout << "Found " << needle << " at " << std::hex << start_addr << std::dec << std::endl;
          start = (uintptr_t)start_addr;
          end = (uintptr_t)end_addr;
          offset = (uintptr_t)offset_addr;
          return;
      } else {
        std::cout << "Not found: " << memory_location << std::endl;
        puts("line: ");
        puts(line);
      }
  }

  std::cout << "Did not find " << needle << std::endl;
}

// hacky function to allocate memory at a given block
// ripped out of c code (which is why it's not idiomatic C++)
void * MemoryOffsets::allocate_at_end(int size) {
  void * addr = mmap(
      (void *)this->end, 
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

// Changes memory permissions so we can write to it
int MemoryOffsets::unprotect() {
  if (mprotect( 
        (void *)this->start, 
        (int)this->end - (int)this->start, 
        PROT_READ | PROT_WRITE | PROT_EXEC) == -1
  ) {
    puts("[!] Couldn't change memory permissions");
    perror("[!] mprotect failed");
    return 1;
  }
  puts("[+] Changed memory permissions successfully!");
  return 0;
}

void overwrite_jump_placeholder(void * addr, int index, uint8_t * buffer) {
  long unsigned int a = (long unsigned int)addr;
  for (int i = 0; i < 4; i++) {
    buffer[index + i] = (uint8_t)((a >> (i * 8)) & 0xFF); // Extract each byte
  }
}

void overwrite_placeholder(void * addr, int index, uint8_t * buffer) {
  long unsigned int a = (long unsigned int)addr;
  for (int i = 0; i < 8; i++) {
    buffer[index + i] = (uint8_t)((a >> (i * 8)) & 0xFF); // Extract each byte
  }
}

int overwrite_addr(void * addr, void * data, int size) {
  std::cout << "[+] Overwriting memory at " << std::hex << addr << std::dec <<  "with size" << size << std::endl;
  if(memcpy(addr, data, size) == NULL) {
    std::cout << "[!] Couldn't overwrite memory" << std::endl;
    return 1;
  }
  std::cout << "[+] Overwrote addr!" << std::endl;
  return 0;
}

bool Hook::install() {
  uint8_t jump_buffer[JUMP_BUFFER_SIZE] = {
    0xE9,       // JMP opcode
    0x00, 0x00, 0x00, 0x00 // (4 bytes for relative address)
  };

  uint8_t relay_buffer[RELAY_BUFFER_SIZE] = {
    0x50,                                                       // push rax
    0x57,                                                       // push rdi
    0x48, 0xB8,                                                 // movabs rax, <hook_func>
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // (8 bytes for hook_func address)
    0xFF, 0xD0,                                                 // call rax
    0x5F,                                                       // pop rdi
    0x58                                                        // pop rax
  };

  overwrite_placeholder(this->hook_func, 4, relay_buffer);
  printf("[+] Prepared relay buffer to hook function at %p\n", this->hook_func);

  void * target_loc = (void *)((char *)*trampoline_cursor - ((char *)((uintptr_t)memo[this->target_memo_index].addr + JUMP_BUFFER_SIZE)));
  overwrite_jump_placeholder(target_loc, 1, jump_buffer);

  printf("[+] Overwriting target function at %p\n", memo[this->target_memo_index].addr);
  printf("[+] Prologue size: %d\n", this->prologue_size);
  printf("[+] Trampoline cursor at %p\n", *trampoline_cursor);

  if (!memcpy((void *)*trampoline_cursor, memo[this->target_memo_index].addr, this->prologue_size)) {
    puts("[!] Couldn't copy prologue size to trampoline");
    return false;
  }
  puts("[+] Copied prologue to trampoline");


  *trampoline_cursor += this->prologue_size;
  puts("[+] Updated trampoline cursor after copying prologue");
  printf("[+] New trampoline cursor at %p\n", *trampoline_cursor);

  if (!memcpy((void *)*trampoline_cursor, relay_buffer, RELAY_BUFFER_SIZE)) {
    puts("[!] Couldn't copy relay to trampoline");
    return false;
  }
  puts("[+] Copied relay to trampoline");

  *trampoline_cursor += RELAY_BUFFER_SIZE;

  if (!memcpy(memo[this->target_memo_index].addr, jump_buffer, JUMP_BUFFER_SIZE)) {
    puts("[!] Couldn't overwrite target function");
    return false;
  }
  puts("[+] Overwrote target function with jump to hook");

  void * return_jump = (void *)((char *)((uintptr_t)memo[this->target_memo_index].addr + this->prologue_size) - ((char *)*trampoline_cursor + JUMP_BUFFER_SIZE));
  uint8_t return_jump_buffer[JUMP_BUFFER_SIZE] = {
    0xE9,       // JMP opcode
    0x00, 0x00, 0x00, 0x00 // (4 bytes for relative address)
  };

  overwrite_jump_placeholder(return_jump, 1, return_jump_buffer);
  if (!memcpy((void *)*trampoline_cursor, return_jump_buffer, JUMP_BUFFER_SIZE)) {
    puts("[!] Couldn't copy return jump to trampoline");
    return false;
  }
  *trampoline_cursor += JUMP_BUFFER_SIZE;

  puts("[+] Copied return jump to trampoline");
  return true;
}


