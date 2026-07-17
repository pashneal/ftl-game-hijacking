#include "memory.h"
#include "FTL.amd64.h"
#include <iostream>
#include <ostream>
#include <cstdio>

#define COMMAND_GUI_CONSTRUCTOR 0
#define CREW_CONTROL_CONSTRUCTOR 1
#define CREW_CONTROL_RBUTTON 2

void hello() {
  std::cout << "Hello, World!" << std::endl;
}

void hello_crew(uintptr_t * addr) {
  std::cout << "(hooked into crew constructor) Hello, crew! Get ready for takeoff!" << std::endl;
  std::cout << "Was pointer " << SharedMemory::crew_constructor << std::endl;
  SharedMemory::crew_constructor = addr;
  std::cout << "Found pointer " << SharedMemory::crew_constructor << std::endl;
  std::cout << 1.0 << std::endl;
}

__attribute__((force_align_arg_pointer)) void intercept_mouse(uintptr_t * addr, int mX, int mY, bool shift) {
  std::cout << "(hooked into mouse intercept) Hello, mouse! Get ready for takeoff!" << std::endl;
  std::cout << "mX " << mX << std::endl;
  std::cout << "mY " << mY << std::endl;
  ShipManager * ship = (ShipManager *) SharedMemory::crew_constructor[0];
  // size of CrewMember
  std::cout << "CrewMember size: " << sizeof(CrewMember) << std::endl;
  std::cout << "CrewMember size: " << std::hex << sizeof(CrewMember) << std::dec << std::endl;
  // size of ShipManager
  std::cout << "ShipManager size: " << sizeof(ShipManager) << std::endl;
  std::cout << "ShipManager size: " << std::hex << sizeof(ShipManager) << std::dec << std::endl;
  
  std::cout << "Crew list size: " << ship->vCrewList.size() << std::endl;
  for (int i = 0; i < ship->vCrewList.size(); i++) {
    std::cout << "Crew member " << i << std::endl;
    CrewMember * crew = ship->vCrewList[i];
    Point position = crew->GetPosition();
    std::cout << "Crew member x " << position.x << std::endl; 
    std::cout << "Crew member y " << position.y << std::endl;
  }
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

  Hook::memo[CREW_CONTROL_CONSTRUCTOR] = {
    "_ZN10CrewControlC2Ev",
    (void*)0x50BCA0,
  };

  Hook::memo[CREW_CONTROL_RBUTTON] = {
    "_ZN11CrewControl7LButtonEiiiib",
    (void*)0x4FAD70,
  };

  Hook * command_gui = new Hook(
      COMMAND_GUI_CONSTRUCTOR, 
      (uintptr_t *)hello,
      6
  );
  Hook * crew_control = new Hook(
      CREW_CONTROL_CONSTRUCTOR, 
      (uintptr_t *)hello_crew,
      7
  );
  Hook * mouse_control = new Hook(
      CREW_CONTROL_RBUTTON, 
      (uintptr_t *)intercept_mouse,
      6
  );

  command_gui->install();
  crew_control->install();
  mouse_control->install();

}
