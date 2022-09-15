

.PHONY: all clean release debug ../blib/blib.a

INCLUDE_DIR:= ../blib/include

include Makefile.config

CFILES :=$(shell echo *.c)
EFILES := $(CFILES:.c=.elf)
BIN_DIR := ../disk_root/bin/

debug: CFLAGS += -g
debug: LDFLAGS += -g
debug: all

release: all 

all: $(EFILES)
	$(MAKE) -C ../blib
	$(MAKE) -C txe
	rm -f $(BIN_DIR)/*
	cp test_threads.elf $(BIN_DIR)/test_threads
	cp sh.elf $(BIN_DIR)/sh
	cp ls.elf $(BIN_DIR)/ls
	cp cat.elf $(BIN_DIR)/cat
	cp clear.elf $(BIN_DIR)/clear
	cp fibo.elf $(BIN_DIR)/fibo
	cp write.elf $(BIN_DIR)/write
	cp test_pipe.elf $(BIN_DIR)/test_pipe
	cp less.elf $(BIN_DIR)/less
	cp txe/txe.elf $(BIN_DIR)/txe


%.elf: %.c
	$(CC) $(CFLAGS) -c $< -o $@.o
	$(LD) -static $@.o ../blib/blib.a -o $@ 

clean:
	rm -rf *.s *.elf
	$(MAKE) -C txe clean

../blib/blib.a:
	$(MAKE) -C ../blib/