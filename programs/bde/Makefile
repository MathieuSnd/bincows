.PHONY: all clean

ELF:=bde.elf
LDLIBS :=../../../sdl_port/build/.libs/libSDL.a ../../blibc/blibc.a

INCLUDE_DIR=../../blibc/include

include ../Makefile.config
CFLAGS += -D_BINCOWS_ -I../../../sdl_port/include/



CFILES := $(shell find ./ -type f -name '*.c')
OBJS:= $(CFILES:.c=.o)


all: $(ELF)
	echo $(OBJS)



$(ELF): $(OBJS)
	$(LD)  $(OBJS) $(LDLIBS) -o $(ELF)

%.o: %.c ../blibc/blibc.a
	$(CC) $(CFLAGS) -c $< -o $@.o


clean: force_look
	rm -rf *.o *.elf


force_look:
	true