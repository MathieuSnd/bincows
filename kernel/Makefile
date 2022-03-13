KERNEL 		:= ../disk_root/boot/kernel.elf
CC  		:= x86_64-elf-gcc
LD          := x86_64-elf-ld
ASM			:= nasm
ASM_FLAGS 	:= -felf64
PY          := python3

BUILDDIR := ./build
 
CFLAGS = -Wall -Wextra -O3 -pipe

INTERNALLDFLAGS :=  -Tlinker.ld            \
					-nostdlib              \
					-Wl,--export-dynamic   \
					-zmax-page-size=0x1000 \
					-static                \
					-pie                   \
					-ztext
#				,--dynamic-linker 

INTERNALCFLAGS  :=  -mgeneral-regs-only    \
					-ffreestanding         \
					-mno-red-zone          \
					-fno-omit-frame-pointer\
					-std=gnu11 			   \
					-Wall -Wextra -Wno-trigraphs \
					-fcompare-debug-second \
					-I/opt/cross/include/  \
					-I../lai/include       \
					-fPIE                  \
					
#					-D BIGGER_FONT


LAI_FILE := ../lai/lai.elf


CFILES := $(shell find ./ -type f -name '*.c')
SFILES := $(shell find ./ -type f -name '*.s')
OBJ    :=  $(SFILES:.s=.s.o) \
		   $(CFILES:.c=.c.o)

OBJ    := $(patsubst ./%, $(BUILDDIR)/%, $(OBJ))



LDSCRIPT := linker.ld 

.PHONY: all clean debug release

debug: INTERNALCFLAGS += -fno-inline -g
debug: all

release: INTERNALCFLAGS += -DNDEBUG
release: all
 
all: $(KERNEL)

 
$(KERNEL): $(OBJ) $(LDSCRIPT) parse_symbols.py
	$(CC) $(INTERNALLDFLAGS) $(OBJ) $(LAI_FILE) -o $@

	rm symbols.txt
	nm ../disk_root/boot/kernel.elf -n >> symbols.txt
	
	$(PY) ./parse_symbols.py
	mv symbols.dat ../disk_root/boot/kernel.symbols

$(BUILDDIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

$(BUILDDIR)/%.s.o: %.s
	mkdir -p $(dir $@)
	$(ASM) $(ASM_FLAGS) $< -o $@
 
clean:
	rm -rf $(KERNEL) $(OBJ)