

.PHONY: all clean debug release

BIN_FILE:=blib.a

CC := x86_64-elf-gcc
AR := x86_64-elf-ar
LD := x86_64-elf-ld
ASM:=nasm



CFLAGS := -O3 -mgeneral-regs-only -Wall -Wextra \
		  -ffreestanding -Iinclude/ -fno-pie -fno-stack-protector -fno-pic

LDFLAGS := -shared -fno-pie -fno-pic

ARFLAGS:=rc

ASM_FLAGS := -felf64


CFILES :=$(shell find src -name "*.c")
SFILES :=$(shell find src -name "*.s")


OBJ :=  $(SFILES:.s=.s.o) \
		$(CFILES:.c=.c.o)


all: $(BIN_FILE)


debug: CFLAGS += -g
debug: LDFLAGS += -g
debug: all
release: all


%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.s.o: %.s
	$(ASM) $(ASM_FLAGS) $< -o $@


$(BIN_FILE): $(OBJ)
#	$(LD) $(LDFLAGS) $(OBJ) -o $(BIN_FILE)
	$(AR) $(ARFLAGS) $(BIN_FILE) $(OBJ)
#	ranlib $(BIN_FILE)

clean:
	rm -rf $(OBJ) *.a *.elf
