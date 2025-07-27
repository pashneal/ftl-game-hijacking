from pwn import *
elf = ELF('./test.out')

# List symbols at program
library = ELF('./over.so')
for key, address in library.symbols.items():
    if "new_" in key:
        print(key, hex(address))
for key, address in elf.symbols.items():
    print(key, hex(address))

elf.asm(elf.symbols['global_func'], 'jmp 0x1119')
elf.save('./test_patched.out')
