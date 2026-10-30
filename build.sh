
gcc -std=c11 -Wall -Wextra ./MAIN/main.c ./BYTE_CODE/chunk.c \
    ./MEMORY/memory.c ./DEBUG/debug.c ./VALUES/value.c \
    ./VIRTUAL_MACHINE/vm.c ./SCANNER/scanner.c \
    ./COMPILER/compiler.c ./STORAGE/storage.c -lreadline -o FreaX
