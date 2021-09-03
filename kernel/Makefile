KERNEL 		:= ../disk_root/boot/kernel.elf
CC  		:= x86_64-elf-gcc
ASM			:= nasm
ASM_FLAGS 	:= -felf64
 
CFLAGS = -Wall -Wextra -O3 -pipe
 
INTERNALLDFLAGS :=     \
	-fno-pic  \
	-Wl,-static,--no-dynamic-linker,-ztext \
	-znocombreloc    \
	-nostdlib      \
	-Tlinker.ld    \
	-z max-page-size=0x1000
 

INTERNALCFLAGS  :=           \
	-znocombreloc \
	-I/opt/cross/include/ \
	-std=gnu11           \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-pic             \
	-mno-80387           \
	-mno-mmx             \
	-mno-3dnow           \
	-mno-sse             \
	-mno-sse2            \
	-mno-red-zone \
	-m64
 
CFILES := $(shell find ./ -type f -name '*.c')
SFILES := $(shell find ./ -type f -name '*.s')
OBJ    :=  $(SFILES:.s=.s.o) $(CFILES:.c=.c.o) charmap.bmp.o
.PHONY: all clean
 
all: $(KERNEL)


 
$(KERNEL): $(OBJ)
	$(CC) $(INTERNALLDFLAGS) $(OBJ) -o $@
 
%.bmp.o: ../resources/bmp/%.bmp
	$(LD) -r -b binary -o $@ $<
%.txt.o: ../resources/ascii/%.txt
	$(LD) -r -b binary -o $@ $<

%.c.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

%.s.o: %.s
	$(ASM) $(ASM_FLAGS) $< -o $@
 
clean:
	rm -rf $(KERNEL) $(OBJ)