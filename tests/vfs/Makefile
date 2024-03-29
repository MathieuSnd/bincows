

.PHONY = run all clean 

EXE = ./vfs.elf
DISKFILE := ./disk.bin

CC  		:= gcc
LD          := gcc
ASM			:= nasm
ASM_FLAGS 	:= -felf64
PY          := python3

KERNEL    := ../../kernel/
KBUILDDIR := $(KERNEL)/build


 
DEFINES := -DDISKFILE=\"$(DISKFILE)\" -DTEST

CFLAGS = -Wall -Wextra -O0 -pipe -I$(KERNEL) $(DEFINES) -fno-inline -g -fstack-protector-all


CFILES := $(shell find ./ -type f -name '*.c')
CFILES += ../tb.c
CFILES += ../disk_tb.c

KCFILES += $(shell find $(KERNEL)/fs  -type f -name '*.c')
KCFILES += $(KERNEL)/drivers/dev.c
KCFILES += $(KERNEL)/drivers/driver.c

KCFILES += $(KERNEL)/lib/logging.c
KCFILES += $(KERNEL)/lib/dump.c
KCFILES += $(KERNEL)/lib/stacktrace.c
KCFILES += $(KERNEL)/lib/panic.c
KCFILES += $(KERNEL)/lib/utf16le.c
KCFILES += $(KERNEL)/lib/string.c
KCFILES += $(KERNEL)/acpi/power.c

OBJ    := $(CFILES:.c=.o) $(patsubst $(KERNEL)/%, $(KBUILDDIR)/%, $(KCFILES:.c=.test.o))



run: all
	sudo $(EXE)


debug: all
	gdb $(EXE)


all: $(EXE)
	cp ../../disk.bin $(DISKFILE)


$(EXE): $(OBJ)
	$(LD) $(OBJ) -o $(EXE)



$(KBUILDDIR)/%.test.o: $(KERNEL)/%.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -rf *.o
	find $(KERNEL) -name "*.test.o" | rm -rf