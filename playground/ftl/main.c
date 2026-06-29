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
#include "socket.h"


#define JUMP_BUFFER_SIZE 5
#define RELAY_BUFFER_SIZE 16

const long unsigned int FTL_BASE_OFFSET = 0x400000; 
const int SELF_PID = -1; // Use -1 to refer to the current process
int * command_gui_addr = NULL;
int * crew_control_addr = NULL;
void * trampoline_cursor = NULL;
static sw_server_t server;


typedef struct {
  void * start_executable_addr;
  void * end_executable_addr;
  long offset;
} mem;

typedef struct {
  char name[64]; // Null terminated name of the symbol 
  void * addr;   // Address of the symbol in memory
} entry;

typedef struct {
  int target_memo_index; // Index in the memo array of the function to overwrite
  void * hook_func;      // Pointer to the hook function (which overwrites the target memo address)
  int prologue_size;    // Number of bytes to copy as prologue
} hook_args;

entry memo[100];
#define WEAPON_CONTROL_KEY_DOWN 0
#define FTL_LOG 1
#define COMMAND_GUI_CONSTRUCTOR 2
#define COMMAND_GUI_KEY_DOWN 3
#define CREW_CONTROL_CONSTRUCTOR 4

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

bool install_hook(hook_args hook, void ** trampoline_cursor) {
  char jump_buffer[JUMP_BUFFER_SIZE] = {
    0xE9,       // JMP opcode
    0x00, 0x00, 0x00, 0x00 // (4 bytes for relative address)
  };

  char relay_buffer[RELAY_BUFFER_SIZE] = {
    0x50,                                                       // push rax
    0x57,                                                       // push rdi
    0x48, 0xB8,                                                 // movabs rax, <hook_func>
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // (8 bytes for hook_func address)
    0xFF, 0xD0,                                                 // call rax
    0x5F,                                                       // pop rdi
    0x58                                                        // pop rax
  };

  overwrite_placeholder(hook.hook_func, 4, relay_buffer);
  printf("[+] Prepared relay buffer to hook function at %p\n", hook.hook_func);

  void * target_loc = (void *)((char *)*trampoline_cursor - ((char *)memo[hook.target_memo_index].addr + JUMP_BUFFER_SIZE));
  overwrite_jump_placeholder(target_loc, 1, jump_buffer);

  printf("[+] Overwriting target function at %p\n", memo[hook.target_memo_index].addr);
  printf("[+] Prologue size: %d\n", hook.prologue_size);
  printf("[+] Trampoline cursor at %p\n", *trampoline_cursor);

  if (!memcpy(*trampoline_cursor, memo[hook.target_memo_index].addr, hook.prologue_size)) {
    puts("[!] Couldn't copy prologue size to trampoline");
    return false;
  }
  puts("[+] Copied prologue to trampoline");


  *trampoline_cursor += hook.prologue_size;

  if (!memcpy(*trampoline_cursor, relay_buffer, RELAY_BUFFER_SIZE)) {
    puts("[!] Couldn't copy relay to trampoline");
    return false;
  }
  puts("[+] Copied relay to trampoline");

  *trampoline_cursor += RELAY_BUFFER_SIZE;

  if (!memcpy(memo[hook.target_memo_index].addr, jump_buffer, JUMP_BUFFER_SIZE)) {
    puts("[!] Couldn't overwrite target function");
    return false;
  }
  puts("[+] Overwrote target function with jump to hook");

  void * return_jump = (void *)((char *)(memo[hook.target_memo_index].addr + hook.prologue_size) - ((char *)*trampoline_cursor + JUMP_BUFFER_SIZE));
  char return_jump_buffer[JUMP_BUFFER_SIZE] = {
    0xE9,       // JMP opcode
    0x00, 0x00, 0x00, 0x00 // (4 bytes for relative address)
  };

  overwrite_jump_placeholder(return_jump, 1, return_jump_buffer);
  if (!memcpy(*trampoline_cursor, return_jump_buffer, JUMP_BUFFER_SIZE)) {
    puts("[!] Couldn't copy return jump to trampoline");
    return false;
  }
  *trampoline_cursor += JUMP_BUFFER_SIZE;

  puts("[+] Copied return jump to trampoline");
  return true;
}


bool ftl_log_wrapper() { 
  typedef bool (*ftl_log_t)(const char*, ...);
  ftl_log_t ftl_log = (ftl_log_t)memo[FTL_LOG].addr;
  ftl_log("Hello World from your hook!");
  return 1;
}

typedef struct {
  bool lower_case_key_press;
  bool upper_case_key_press;
  bool enter_key_press;
  char key;
} command_t;

void parse_command(char * buffer, int buffer_size, command_t * command) {
  command->lower_case_key_press = false;
  command->upper_case_key_press = false;
  command->enter_key_press = false;

  if (buffer_size < 2) { return; }
  if (buffer[0] == '.') {
    command->lower_case_key_press = true;
    command->key = buffer[1];
  }
  if (buffer[0] == '>'){
    command->upper_case_key_press = true;
    command->key = buffer[1];
  }
}

void sw_callback(
    char * input_buffer, 
    int input_size, 
    char * output_buffer, 
    int * output_size) 
{
  printf("[+] calling back and echoing with input %s\n", input_buffer);
  memcpy(output_buffer, input_buffer, input_size);
  typedef bool (*command_key_down_t)(int *, char, bool);
  command_key_down_t command_key_down = (command_key_down_t)memo[COMMAND_GUI_KEY_DOWN].addr;

  command_t command;
  parse_command(input_buffer, input_size, &command);

  if (command.lower_case_key_press) {
    command_key_down(command_gui_addr, command.key, false); 
  } else if (command.upper_case_key_press) {
    command_key_down(command_gui_addr, command.key, true);
  }
  *output_size  = input_size;
}

void command_gui_wrapper(int *addr) {
  puts("[+] Hooked CommandGui constructor!");
  printf("[+] captured this pointer: %p\n", addr);
  command_gui_addr = addr;
  if (sw_start(&server, 8080, sw_callback) != 0) {
    puts("[!] Could not start new socket server");
  }
  puts("[+] jumping back to original CommandGui constructor!");
}

void crew_control_constructor_wrapper(int *addr) {
  puts("[+] Hooked CrewControl constructor!");
  printf("[+] captured addr pointer: %p\n", addr);
  crew_control_addr = addr;
  puts("[+] jumping back to original CrewControl constructor!");
}

int unprotect(mem * memory) {
  if (mprotect( memory->start_executable_addr, 0x42a000, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
    puts("[!] Couldn't change memory permissions");
    perror("[!] mprotect failed");
    return 1;
  }
  puts("[+] Changed memory permissions successfully!");
  return 0;
}


__attribute__((constructor)) int hook() {
  

  // Allocate a page near the target memory
  mem * mem_offsets = (mem *)malloc(sizeof(mem));
  if (search_mem(SELF_PID, "FTL", mem_offsets) != 0) { return 1; }

  if (unprotect(mem_offsets) != 0) { return 1; }

  // make sure we can edit memory to our heart's content
  void * allocated = allocate_near(mem_offsets, 0x20000); 
  trampoline_cursor = allocated;

  printf("[+] Trampoline cursor starts at %p\n", trampoline_cursor);



  // Memo entries
  memo[WEAPON_CONTROL_KEY_DOWN] = (entry){
    "_ZN13WeaponControl7KeyDownEi",
    (void*)0x596920,
  };
  memo[FTL_LOG] = (entry){
    "_Z7ftl_logPKcz",
    (void*)0x5A2380,
  };
  memo[COMMAND_GUI_CONSTRUCTOR] = (entry){
    "_ZN10CommandGuiC2Ev",
    (void*)0x500150,
  };
  memo[COMMAND_GUI_KEY_DOWN] = (entry){
    "_ZN10CommandGui7KeyDownEib",
    (void*)0x4FD110,
  };
  memo[CREW_CONTROL_CONSTRUCTOR] = (entry){
    "_ZN11CrewControlC2Ev",
    (void*)0x50BCA0,
  };

  // Install hooks
  hook_args command_gui_hook = {
    COMMAND_GUI_CONSTRUCTOR,
    (void *)command_gui_wrapper,
    0x06, 
  };

  hook_args crew_control_constructor_hook = {
    CREW_CONTROL_CONSTRUCTOR,
    (void *)crew_control_constructor_wrapper,
    0x07,
  };

  if (!install_hook(command_gui_hook, &trampoline_cursor)) {
    puts("[!] Couldn't install command gui hook");
    return 1;
  }

  if (!install_hook(crew_control_constructor_hook, &trampoline_cursor)) {
    puts("[!] Couldn't install crew control hook");
    return 1;
  }

  puts("[+] Starting hooks...");


  return 0;
}
