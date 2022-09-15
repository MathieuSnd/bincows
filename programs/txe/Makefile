.PHONY: all clean

ELF:=txe.elf

INCLUDE_DIR=../../blib/include

include ../Makefile.config


INCLUDE_DIR=../$(INCLUDE_DIR)

CFILES := $(shell find ./ -type f -name '*.c')
OBJS:= $(CFILES:.c=.o)


all: $(ELF)
	echo $(OBJS)

$(ELF): $(OBJS)
	$(LD) -static $(OBJS) ../../blib/blib.a -o $(ELF)

%.o: %.c ../blib/blib.a
	$(CC) $(CFLAGS) -c $< -o $@.o


clean:
	rm -rf *.o *.elf