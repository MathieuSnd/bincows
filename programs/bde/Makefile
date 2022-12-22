.PHONY: all clean

ELF:=bde.elf
LDLIBS :=$(SDL2_BASE)/libSDL2.a ../../blibc/blibc.a

INCLUDE_DIR=../../blibc/include

MAKE := make
CC     := $(GNU_PREFIX)x86_64-elf-gcc
LD     := $(GNU_PREFIX)x86_64-elf-ld


CFLAGS := -O3 -fno-inline \
		  -I$(INCLUDE_DIR) -fno-omit-frame-pointer -fno-pie --freestanding -g




CFLAGS += -D__BINCOWS__ -I$(SDL2_BASE)/include/
CFLAGS += -march=native



CFILES := $(shell find ./ -type f -name '*.c')
OBJS:= $(CFILES:.c=.o)


all: $(ELF)


host:
	cc $(CFILES) -O0 -msse4 -mavx2 -g -o linux.elf -lpthread -lSDL2

host-dbg: host
	gdb --args linux.elf input.c




$(ELF): $(OBJS)
	$(LD)  $(OBJS) $(LDLIBS) -o $(ELF)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


clean: force_look
	rm -rf *.o *.elf


force_look:
	true