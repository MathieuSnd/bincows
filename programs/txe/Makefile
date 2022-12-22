.PHONY: all clean

ELF:=txe.elf
LIB:=../../blibc/blibc.a

INCLUDE_DIR=../../blibc/include

include ../Makefile.config
CFLAGS += -D__BINCOWS__



CFILES := $(shell find ./ -type f -name '*.c')
OBJS:= $(CFILES:.c=.o)


all: $(ELF)
	echo $(OBJS)

host:
	cc $(CFILES) -O2 -g -o linux.elf -lpthread

host-dbg: host
	gdb --args linux.elf input.c


$(ELF): $(OBJS)
	$(LD)  $(OBJS) $(LIB) -o $(ELF)

%.o: %.c ../blibc/blibc.a
	$(CC) $(CFLAGS) -c $< -o $@.o


clean:
	rm -rf *.o *.elf