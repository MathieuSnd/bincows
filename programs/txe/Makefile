.PHONY: all clean

ELF:=txe.elf

INCLUDE_DIR=../../blibc/include

include ../Makefile.config


INCLUDE_DIR=../$(INCLUDE_DIR)

CFILES := $(shell find ./ -type f -name '*.c')
OBJS:= $(CFILES:.c=.o)


all: $(ELF)
	echo $(OBJS)

$(ELF): $(OBJS)
	$(LD) -static $(OBJS) ../../blibc/blibc.a -o $(ELF)

%.o: %.c ../blibc/blibc.a
	$(CC) $(CFLAGS) -c $< -o $@.o


clean:
	rm -rf *.o *.elf