#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include <cstdint>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>

#define SELF_PID -1
#define JUMP_BUFFER_SIZE 5
#define RELAY_BUFFER_SIZE 16

class MemoryOffsets {
  public:
    uintptr_t start = 0;
    uintptr_t end = 0;
    uintptr_t offset = 0;

    MemoryOffsets(std::string needle);
    void * allocate_at_end(int size);
    int unprotect();
};

struct entry {
  char name[64]; // Null terminated name of the symbol 
  void * addr;   // Address of the symbol in memory
};
    

class Hook {
  public:
    int target_memo_index;
    uintptr_t * hook_func;
    int prologue_size;
    static entry memo[100]; // Array to store the memoized entries
    static char ** trampoline_cursor; // Pointer to the trampoline cursor

    Hook(int target_memo_index, uintptr_t * hook_func, int prologue_size){
      this->target_memo_index = target_memo_index;
      this->hook_func = hook_func;
      this->prologue_size = prologue_size;
    };
    bool install();
};

int overwrite_addr(void * addr, void * data, int size);
#endif // MEMORY_H
