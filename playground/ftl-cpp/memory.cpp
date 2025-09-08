#include "memory.h"
#include <cstring>
#include <iostream>
#include <ios>
#include <ostream>

MemoryOffsets::MemoryOffsets(std::string needle) {
  std::string mapsPath = "/proc/self/maps";
  FILE* mapsFile = fopen(mapsPath.c_str(), "r");

  if (mapsFile == NULL) { return; }

  char memory_location[400];
  long unsigned int *start_addr;
  long unsigned int *end_addr;
  long unsigned int *offset_addr;
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
      }
  }

  std::cout << "Did not find " << needle << std::endl;
}


