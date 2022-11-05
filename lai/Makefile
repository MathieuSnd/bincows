CC  		:= x86_64-elf-gcc
LD  		:= x86_64-elf-ld
 
CFLAGS = -Wall -Wextra -O3 -pipe -Ilai/include

INTERNALLDFLAGS :=  -nostdlib              \
					-zmax-page-size=0x1000 \
					-static                \
					-ztext				  \
					-relocatable
#				,--dynamic-linker 

INTERNALCFLAGS  :=  -mgeneral-regs-only \
					-ffreestanding      \
					-mno-red-zone       \
					-mno-80387           \
					-fno-omit-frame-pointer \
					-std=gnu11 			\
					-Wall -Wextra       \
					-fcompare-debug-second \
					-I/opt/cross/include/  \
					-fpie                  \
 
CFILES := $(shell find ./lai -type f -name '*.c')
SFILES := $(shell find ./lai -type f -name '*.s')
OBJ    :=  $(SFILES:.s=.s.o) $(CFILES:.c=.c.o)

LDSCRIPT := linker.ld 
#charmap.bmp.o
.PHONY: all clean
 
OUTPUT := lai.elf


all: $(OUTPUT)

clean:
	rm -rf $(OBJ) $(OUTPUT)




 
$(OUTPUT): $(OBJ)
	$(LD) $(INTERNALLDFLAGS) $(OBJ) -o $@
 

%.c.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@
 