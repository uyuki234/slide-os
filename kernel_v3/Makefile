CC      = i686-elf-gcc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -std=c11
OBJS    = boot.o arch.o interrupts.o serial.o kprintf.o timer.o keyboard.o \
          mouse.o gfx.o window.o kernel.o

all: kernel.elf

%.o: %.s
	nasm -f elf32 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ヘッダを変えたら全.oを作り直す(依存関係追跡の簡易版)
$(OBJS): $(wildcard *.h)

kernel.elf: $(OBJS) linker.ld
	$(CC) -T linker.ld -ffreestanding -nostdlib -o $@ $(OBJS) -lgcc

# GRUB入り起動ISO(El Torito)。QEMUの-kernel直接ロードはMultibootの
# ビデオ要求を無視するので、フレームバッファが欲しければISO経由が必須
slideos.iso: kernel.elf grub.cfg
	rm -rf isodir
	mkdir -p isodir/boot/grub
	cp kernel.elf isodir/boot/
	cp grub.cfg isodir/boot/grub/
	i686-elf-grub-mkrescue -o $@ isodir

iso: slideos.iso

run-iso: slideos.iso
	qemu-system-i386 -cdrom slideos.iso -vga std -serial stdio

run: kernel.elf  # 防御チェックの確認用(グラフィックス無しで止まるのが正常)
	qemu-system-i386 -kernel kernel.elf -serial stdio

clean:
	rm -f *.o *.elf *.iso
	rm -rf isodir
