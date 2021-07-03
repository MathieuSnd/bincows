KERNEL := ../disk_root/boot/kernel.elf
CC = x86_64-elf-gcc
 
CFLAGS = -Wall -Wextra -O2 -pipe
 
INTERNALLDFLAGS :=     \
	-fno-pic -fpie \
	-Wl,-static,-pie,--no-dynamic-linker,-ztext \
	-static-pie    \
	-nostdlib      \
	-Tlinker.ld    \
	-z max-page-size=0x1000
 
INTERNALCFLAGS  :=           \
	-I/opt/cross/include/ \
	-H \
	-std=gnu11           \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-pic -fpie       \
	-mno-80387           \
	-mno-mmx             \
	-mno-3dnow           \
	-mno-sse             \
	-mno-sse2            \
	-mno-red-zone
 
CFILES := $(shell find ./ -type f -name '*.c')
OBJ    := $(CFILES:.c=.o)
 
.PHONY: all clean
 
all: $(KERNEL)
 
$(KERNEL): $(OBJ)
	$(CC) $(INTERNALLDFLAGS) $(OBJ) -o $@
 
%.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@
 
clean:
	rm -rf $(KERNEL) $(OBJ)