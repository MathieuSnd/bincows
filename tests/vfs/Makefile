

.PHONY = run all clean 

EXE = ./vfs

CC  		:= gcc
LD          := gcc
ASM			:= nasm
ASM_FLAGS 	:= -felf64
PY          := python3

KERNEL=../../kernel/

 
CFLAGS = -Wall -Wextra -O3 -pipe -I$(KERNEL)


CFILES := $(shell find ./ -type f -name '*.c')
CFILES := ../tb.c

KCFILES += $(shell find $(KERNEL)/fs  -type f -name '*.c')
KCFILES += $(KERNEL)/lib/logging.c


OBJ    := $(CFILES:.c=.test.o) $(KCFILES:.c=.test.o) 


run: all
	$(EXE)


all: $(EXE)


$(EXE): $(OBJ)
	$(LD) $(OBJ) -o $(EXE)



%.test.o: %.c
	$(CC) -DTEST $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@


clean:
	rm -rf *.o
	rm -rf $(KERNEL)*.test.o