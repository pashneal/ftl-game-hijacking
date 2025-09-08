#include "memory.h"
#include <iostream>
#include <ostream>

__attribute__((constructor)) int hook() {
  std::cout << "Hello, World!" << std::endl;
  MemoryOffsets * mem = new MemoryOffsets("FTL");
  std::cout << "MemoryOffsets created" << std::endl;

  mem->allocate_at_end(0x20000);
  return 0;
}
