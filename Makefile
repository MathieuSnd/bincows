
.PHONY: clean all run_qemu disk kernel force_look threaded_compile\
	    native_disk_simu local_disk_install test programs lib remake lai


IMAGE_FILE            ?= disk.bin
USED_LOOPBACK         ?= /dev/loop2
QEMU_PATH             ?= qemu-system-x86_64
QEMU_RAM_MB           ?= 256
QEMU_LOG_FILE         ?= qemu.log
QEMU_BIOS_FILE        ?= /usr/share/ovmf/OVMF.fd
QEMU_GRAPHICS_OPTIONS ?= -vga virtio
QEMU_EXTRA_OPTIONS    ?=


CC := $(GNU_PREFIX)x86_64-elf-gcc
LD := $(GNU_PREFIX)x86_64-elf-ld


# default: passing -g -fno-inline
# to gcc
# you can change this to 
# 'release' to replace by
# NDEBUG
KERNEL_TARGET ?= debug



QEMU_ARGS := -bios $(QEMU_BIOS_FILE)   \
			 -m    $(QEMU_RAM_MB)      \
			 -D    $(QEMU_LOG_FILE)    \
			 $(QEMU_GRAPHICS_OPTIONS)  \
			 -M q35                    \
			 -cpu host \
			 -accel kvm \
			 -monitor stdio            \
			 -no-reboot  -no-shutdown  \
			 -device nvme,drive=NVME1,serial=deadbeef \
			-drive format=raw,if=none,id=NVME1,file=$(IMAGE_FILE) \
			 $(QEMU_EXTRA_OPTIONS)
			
# -cpu Icelake-Server \

run_qemu: compile image_file local_disk_install
	$(QEMU_PATH) $(QEMU_ARGS)


compile: lib programs kernel


partition_install:
	./write_disk.sh

native_disk_simu: compile
	sudo $(QEMU_PATH) $(QEMU_ARGS) -device nvme,drive=NVME2,serial=dead \
			-drive format=raw,if=none,id=NVME2,file=/dev/nvme0n1


compile: lib programs


test: force_look
	$(MAKE) -C ./tests/


debug: compile
	$(QEMU_PATH) $(QEMU_DEBUG_ARGS)


programs: lib force_look
	GNU_PREFIX=$(GNU_PREFIX) make -C programs

lib: force_look
	GNU_PREFIX=$(GNU_PREFIX) make -C blibc

threaded_compile:
	make -j compile


$(IMAGE_FILE): kernel
	dd if=/dev/zero bs=1M count=0 seek=1024 of=$(IMAGE_FILE)
	sudo /sbin/parted -s $(IMAGE_FILE) mklabel gpt
	sudo /sbin/parted -s $(IMAGE_FILE) mkpart Bincows fat32 0% 100%
	sudo /sbin/parted -s $(IMAGE_FILE) set 1 esp on


image_file: kernel $(IMAGE_FILE)
	sudo losetup -P $(USED_LOOPBACK) $(IMAGE_FILE)
	
	sudo /sbin/mkfs.fat -F 32 $(USED_LOOPBACK)p1 -s 8
	mkdir -p img_mount
	sudo mount $(USED_LOOPBACK)p1 img_mount

	sudo cp -r build/* img_mount/
	sync
	sudo umount img_mount
	
	sudo /sbin/losetup -d $(USED_LOOPBACK)
	rm -rf img_mount



remount_disk:
	sudo losetup -P $(USED_LOOPBACK) $(IMAGE_FILE)
	
	mkdir -p ./disk_output

	sudo mount $(USED_LOOPBACK)p1 ./disk_output/


demount_disk:
	sudo umount ./disk_output/

	sudo /sbin/losetup -d $(USED_LOOPBACK)
	rmdir ./disk_output



kernel: lai force_look
	$(MAKE) -C ./kernel/ $(KERNEL_TARGET)

clean:
	$(MAKE) clean -C blibc
	$(MAKE) clean -C programs
	$(MAKE) clean -C kernel
	rm -f $(IMAGE_FILE)
	rm -rf build
	rm -rf lai/build

force_look:
	true


# copy UEFI limine files into build
limine_efi_install:
	LIMINE_VERSION=2.85 ./update_limine.sh


# create build/ tree: main FS layout 
all:
	mkdir -p ./build/
	mkdir -p ./build/bin
	mkdir -p ./build/etc
	mkdir -p ./build/var/log
	mkdir -p ./build/boot
	mkdir -p ./build/EFI/BOOT
	mkdir -p ./build/home

	cp ./resources/ascii/boot_message.txt ./build/boot
	cp ./resources/cfg/limine.cfg ./build/boot/
	cp ./resources/cfg/default.shrc ./build/home/.shrc
	echo "" > ./build/home/testfile

	
	make limine_efi_install
	make programs
	make kernel



#LDFLAGS := -shared -fno-pie -fno-pic
CFLAGS := -O3 -mgeneral-regs-only -Wall -Wextra -mno-red-zone \
			-ffreestanding -Iinclude/ -fno-pie -fno-stack-protector -fno-pic -g

lai: 
	make -C lai