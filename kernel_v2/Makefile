CC      = i686-elf-gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -std=c11
OBJS    = boot.o arch.o interrupts.o kernel.o timer.o keyboard.o kprintf.o window.o

all: kernel.elf

# ヘッダを変えたら全.oを作り直す(依存関係追跡の簡易版)
$(OBJS): $(wildcard *.h)

%.o: %.s
	nasm -f elf32 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJS) linker.ld
	$(CC) -T linker.ld -ffreestanding -nostdlib -o $@ $(OBJS) -lgcc

run: kernel.elf
	qemu-system-i386 -kernel kernel.elf

clean:
	rm -f *.o *.elf
