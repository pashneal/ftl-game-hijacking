#include "memory.h"
#include <iostream>
#include <ostream>

__attribute__((constructor)) int hook() {
  std::cout << "Hello, World!" << std::endl;
  MemoryOffsets * mem = new MemoryOffsets("FTL");
  std::cout << "MemoryOffsets created" << std::endl;
  return 0;
}
