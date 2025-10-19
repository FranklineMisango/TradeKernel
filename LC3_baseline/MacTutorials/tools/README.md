# Hello World Assembly Program

This directory contains a simple "Hello, World!" program written in x86-64 assembly.

## Running on macOS

The current version is configured for macOS system calls.

### Prerequisites
- NASM assembler (install with `brew install nasm`)
- GCC (usually pre-installed on macOS)

### Build and Run
```bash
# Assemble
nasm -f macho64 hello.asm -o hello.o

# Link
gcc -Wl,-no_pie hello.o -o hello

# Run
./hello
```

## Running on Linux

For Linux, the code needs to use Linux system calls. Modify `hello.asm` to use the Linux version:

```asm
section .data
    hello:     db 'Hello, World!',10
    helloLen:  equ $-hello

section .text
    global _start

_start:
    mov eax,4            ; sys_write
    mov ebx,1            ; stdout
    mov ecx,hello
    mov edx,helloLen
    int 80h              ; Linux syscall
    mov eax,1            ; sys_exit
    mov ebx,0
    int 80h
```

### Prerequisites
- NASM assembler (install with `sudo apt install nasm`)

### Build and Run
```bash
# Assemble
nasm -f elf64 hello.asm -o hello.o

# Link
ld hello.o -o hello

# Run
./hello
```

## Output
Both versions will print: `Hello, World!`