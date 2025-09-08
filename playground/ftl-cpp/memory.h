#ifndef MEMORY_H
#define MEMORY_H

#include <string>
#include <cstdint>

class MemoryOffsets {
  public:
    uintptr_t start = 0;
    uintptr_t end = 0;
    uintptr_t offset = 0;

    MemoryOffsets(std::string needle);
};

#endif // MEMORY_H
