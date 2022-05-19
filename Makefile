IMG=out/trajan.img
VMI=out/trajan.hdd

KERNEL=trajan.elf
BUILDDIR=build

ROOTDIR=root
TEMPDIR=/tmp/trajan_dir

KVM=--enable-kvm -cpu host
LOG=debug/debug.log

LOOPBACK=debug/loopback_dev

EMU=qemu-system-x86_64
ENUMA=-numa node,cpus=0,nodeid=0 \
	  -numa node,cpus=1,nodeid=1 \
	  -numa node,cpus=2,nodeid=2 \
	  -numa node,cpus=3,nodeid=3
KVM=--enable-kvm -cpu host
EFLAGS=-smp 6 -s -S -d int -boot c -m 8G -nic user,model=e1000 -serial file:$(LOG) -monitor stdio -no-reboot -no-shutdown -machine q35 -hda $(IMG)

all: iso

build:
	mkdir $(BUILDDIR) && cd $(BUILDDIR) && meson .. && ninja

iso: $(BUILDDIR)
	rm -f $(IMG)
	dd if=/dev/zero bs=1G count=2 of=$(IMG)
	parted -s $(IMG) mklabel msdos
	parted -s $(IMG) mkpart primary 1 50%
	parted -s $(IMG) mkpart primary 50% 100%
	echfs-utils -m -p0 $(IMG) quick-format 512
	echfs-utils -m -p0 $(IMG) import misc/limine.cfg limine.cfg
	echfs-utils -m -p0 $(IMG) import misc/limine.sys limine.sys
	(cd misc && ls | cpio -ov > ./initrd)
	echfs-utils -m -p0 $(IMG) import misc/initrd initrd
	echfs-utils -m -p0 $(IMG) import $(BUILDDIR)/$(KERNEL) $(KERNEL)
	echo "t\n2\nef\n\nw\nq\n" | fdisk $(IMG)
	sudo losetup -f >> $(LOOPBACK)
	sudo losetup `cat $(LOOPBACK)` $(IMG)
	sudo partprobe `cat $(LOOPBACK)`
	sudo mkfs.fat -F 32 `cat $(LOOPBACK)`p2
	mkdir -p $(TEMPDIR)
	sudo mount `cat $(LOOPBACK)`p2 $(TEMPDIR)
	sudo mkdir -p $(TEMPDIR)/EFI/BOOT
	sudo cp $(BUILDDIR)/$(KERNEL) $(TEMPDIR)
	sudo cp misc/limine.efi $(TEMPDIR)/EFI/BOOT/BOOTX64.EFI
	sudo umount $(TEMPDIR)
	sudo losetup -d `cat $(LOOPBACK)`
	misc/limine-install $(IMG)

kvm: iso
	$(EMU) $(KVM) $(EFLAGS)

kvm-efi: iso
	$(EMU) -bios /usr/share/qemu/OVMF.fd $(KVM) $(EFLAGS)

kvm-numa: iso
	$(EMU) $(ENUMA) $(KVM) $(EFLAGS)

run: iso
	$(EMU) $(EFLAGS)

run-efi: iso
	$(EMU) -bios /usr/share/qemu/OVMF.fd $(EFLAGS)

run-numa: iso
	$(EMU) $(ENUMA) $(EFLAGS)

.PHONY : clean iso run
clean:
	rm -f $(OBJS)
	rm -f $(KERNEL)
	rm -f $(BINS)
	rm -f $(IMG)
	rm -f $(VMI)
	rm -rf $(BUILDDIR)
	rm -f $(LOOPBACK)
