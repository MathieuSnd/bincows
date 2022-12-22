

.PHONY: all clean release debug txe

INCLUDE_DIR:= ../blibc/include

include Makefile.config

CFILES :=$(shell echo *.c)
EFILES := $(CFILES:.c=.elf)
BIN_DIR := ../build/bin/




CFLAGS += -D__BINCOWS__

debug: CFLAGS += -g
debug: LDFLAGS += -g
debug: all

release: all 

all: $(EFILES) txe bde
	rm -f $(BIN_DIR)/*
	cp test_threads.elf $(BIN_DIR)/test_threads
	cp sh.elf $(BIN_DIR)/sh
	cp ls.elf $(BIN_DIR)/ls
	cp cat.elf $(BIN_DIR)/cat
	cp clear.elf $(BIN_DIR)/clear
	cp fibo.elf $(BIN_DIR)/fibo
	cp write.elf $(BIN_DIR)/write
	cp sleep.elf $(BIN_DIR)/sleep
	cp test_pipe.elf $(BIN_DIR)/test_pipe
	cp less.elf $(BIN_DIR)/less
	cp init.elf $(BIN_DIR)/init
	cp echo.elf $(BIN_DIR)/echo
	cp txe/txe.elf $(BIN_DIR)/txe
	cp bde/bde.elf $(BIN_DIR)/bde

%.elf: %.c
	$(CC) $(CFLAGS) -c $< -o $@.o
	$(LD) -static $@.o $(LDLIBS) -o $@ 

clean:
	rm -rf *.s *.elf
	$(MAKE) -C bde clean
	$(MAKE) clean -C txe


txe: force_look
	$(MAKE) -C txe

bde: force_look
	$(MAKE) -C bde


force_look: 
	true



#../blibc/blibc.a:

../blibc/blibc.a: ../blibc/blibc.a
	$(MAKE) -C ../blibc/