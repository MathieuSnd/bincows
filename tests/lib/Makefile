

.PHONY = run all clean 

EXE = ./vfs
DISKFILE := ./disk.bin

CC  		:= gcc
LD          := gcc
ASM			:= nasm
ASM_FLAGS 	:= -felf64
PY          := python3

KERNEL=../../kernel/

 
DEFINES := -DDISKFILE=\"$(DISKFILE)\" -DTEST

CFLAGS = -Wall -Wextra -O0 -pipe -I$(KERNEL) $(DEFINES) -fno-inline -g -no-stdlib


CFILES := $(shell find ./ -type f -name '*.c')
CFILES += ../tb.c

KCFILES += $(KERNEL)/lib/string.c


OBJ    := $(CFILES:.c=.o) $(KCFILES:.c=.test.o) 



run: all
	$(EXE)


debug: all
	gdb $(EXE)


all: $(EXE)
	cp ../../disk.bin $(DISKFILE)


$(EXE): $(OBJ)
	$(LD) $(OBJ) -o $(EXE)



%.test.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -rf *.o
	find $(KERNEL) -name "*.test.o" | rm -rf