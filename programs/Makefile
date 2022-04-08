

.PHONY: all clean


CC := x86_64-elf-gcc
LD := x86_64-elf-ld

CFLAGS := -O3 -fno-inline -mno-red-zone -mgeneral-regs-only \
		  -ffreestanding -I../blib/include -fno-omit-frame-pointer -fno-pie



CFILES :=$(shell find -name "*.c")
EFILES := $(CFILES:.c=.elf)

all: $(EFILES)
	cp *.elf ../disk_root/bin/




%.elf: %.c ../blib/blib.a
	$(CC) $(CFLAGS) -c $< -o $@.o
#	$(LD) -static -L../blib/ -lblib $@.o -o $@ 
	$(LD) -static $@.o ../blib/blib.a -o $@ 

clean:
	rm -rf *.s *.elf
