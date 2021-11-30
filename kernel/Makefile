KERNEL 		:= ../disk_root/boot/kernel.elf
CC  		:= x86_64-elf-gcc
ASM			:= nasm
ASM_FLAGS 	:= -felf64
 
CFLAGS = -Wall -Wextra -O3 -pipe

INTERNALLDFLAGS :=  -Tlinker.ld            \
					-nostdlib              \
					-Wl,--export-dynamic \
					-zmax-page-size=0x1000 \
					-static                \
					-pie                   \
					-ztext
#				,--dynamic-linker 

INTERNALCFLAGS  :=  -mgeneral-regs-only \
					-ffreestanding      \
					-mno-red-zone       \
					-mno-80387           \
					-fno-omit-frame-pointer \
					-std=gnu11 			\
					-Wall -Wextra       \
					-fcompare-debug-second \
					-I/opt/cross/include/  \
					-fpie                  \
#					-D BIGGER_FONT
 
CFILES := $(shell find ./ -type f -name '*.c')
SFILES := $(shell find ./ -type f -name '*.s')
OBJ    :=  $(SFILES:.s=.s.o) $(CFILES:.c=.c.o) 
#charmap.bmp.o
.PHONY: all clean
 
all: $(KERNEL)


 
$(KERNEL): $(OBJ)
	$(CC) $(INTERNALLDFLAGS) $(OBJ) -o $@
 
#%.bmp.o: ../resources/bmp/%.bmp
#	$(LD) -r -b binary -o $@ $<
#%.txt.o: ../resources/ascii/%.txt
#	$(LD) -r -b binary -o $@ $<


kernel/memory/paging.o: INTERNALCFLAGS += -Wno-int-conversion
	

%.c.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

%.s.o: %.s
	$(ASM) $(ASM_FLAGS) $< -o $@
 
clean:
	rm -rf $(KERNEL) $(OBJ)