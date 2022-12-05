

.PHONY: all clean release debug txe

INCLUDE_DIR:= ../blibc/include

include Makefile.config

CFILES :=$(shell echo *.c)
EFILES := $(CFILES:.c=.elf)
BIN_DIR := ../build/bin/
LDLIBS := ../blibc/blibc.a


CFLAGS += -D_BINCOWS_

debug: CFLAGS += -g
debug: LDFLAGS += -g
debug: all

release: all 

all: $(EFILES) txe
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
	cp de.elf $(BIN_DIR)/de
	cp init.elf $(BIN_DIR)/init
	cp txe/txe.elf $(BIN_DIR)/txe


de.elf: LDLIBS := ../../sdl_port/build/.libs/libSDL.a $(LDLIBS)
de.elf: CFLAGS += -I../../sdl_port/include

%.elf: %.c
	$(CC) $(CFLAGS) -c $< -o $@.o
	$(LD) -static $@.o $(LDLIBS) -o $@ 

clean:
	rm -rf *.s *.elf
	$(MAKE) -C txe clean


txe: force_look
	$(MAKE) -C txe


force_look: 
	true



../blibc/blibc.a:

#../blibc/blibc.a: ../blibc/blibc.a
#	$(MAKE) -C ../blibc/