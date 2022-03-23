

.PHONY: all

CC := x86_64-elf-gcc
CFLAGS := -O0 -fno-inline -mno-red-zone -mgeneral-regs-only \
		  -ffreestanding


CFILES :=$(shell find -name "*.c")
EFILES := $(CFILES:.c=.elf)

all: $(EFILES)
	cp *.elf ../disk_root/bin/

%.elf: %.c
	$(CC) $(CFLAGS) -c $< -o $@ 


clean:
	rm -rf *.s *.elf
