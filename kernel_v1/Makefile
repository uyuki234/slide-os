CC      = i686-elf-gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -std=c11

all: kernel.elf

boot.o: boot.s
	nasm -f elf32 $< -o $@

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: boot.o kernel.o linker.ld
	$(CC) -T linker.ld -ffreestanding -nostdlib -o $@ boot.o kernel.o -lgcc

run: kernel.elf
	qemu-system-i386 -kernel kernel.elf

clean:
	rm -f *.o *.elf
