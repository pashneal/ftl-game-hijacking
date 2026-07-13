#include "memory.h"
#include <iostream>
#include <ostream>

#define COMMAND_GUI_CONSTRUCTOR 0


void hello() {
  std::cout << "Hello, World!" << std::endl;
}

__attribute__((constructor)) void hook() {
  std::cout << "Hello, World!" << std::endl;
  MemoryOffsets * mem = new MemoryOffsets("FTL");
  std::cout << "MemoryOffsets created" << std::endl;
  mem->unprotect();

  void * trampoline_cursor = mem->allocate_at_end(0x20000);
  Hook::memo[COMMAND_GUI_CONSTRUCTOR] = {
    "_ZN10CommandGuiC2Ev",
    (void*)0x500150,
  };
  Hook::trampoline_cursor = (char **)&trampoline_cursor;

  Hook * crew_control = new Hook(
      COMMAND_GUI_CONSTRUCTOR, 
      (uintptr_t *)hello,
      5
  );

  crew_control->install();

}
