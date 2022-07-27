

.PHONY: all clean release debug


CC := x86_64-elf-gcc
LD := x86_64-elf-ld

CFLAGS := -O1 -fno-inline -mno-red-zone -mgeneral-regs-only \
		  -I../blib/include -fno-omit-frame-pointer -fno-pie



CFILES :=$(shell echo *.c)
EFILES := $(CFILES:.c=.elf)
BIN_DIR := ../disk_root/bin/

debug: CFLAGS += -g
debug: LDFLAGS += -g
debug: all

release: all 

all: $(EFILES)
	cp shell.elf $(BIN_DIR)/sh
	cp ls.elf $(BIN_DIR)/ls
	cp cat.elf $(BIN_DIR)/cat
	cp clear.elf $(BIN_DIR)/clear
	cp fibo.elf $(BIN_DIR)/fibo
	cp write.elf $(BIN_DIR)/write
	cp test_pipe.elf $(BIN_DIR)/test_pipe


%.elf: %.c ../blib/blib.a
	$(CC) $(CFLAGS) -c $< -o $@.o
#	$(LD) -static -L../blib/ -lblib $@.o -o $@ 
	$(LD) -static $@.o ../blib/blib.a -o $@ 

clean:
	rm -rf *.s *.elf
