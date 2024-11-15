IMG=build/hades.img

KERNEL=build/hades.elf
BUILDDIR=build

ROOTDIR=root

KVM=--enable-kvm -cpu host
LOG=build/qemu.log

NETFLAGS=-nic tap,ifname=tap0,script=no,downscript=no,model=e1000
EMU=qemu-system-x86_64
KVM=--enable-kvm -cpu host
EFLAGS=--s -S -d int -boot c -m 2G $(NETFLAGS) -serial file:$(LOG) -monitor stdio -no-reboot -no-shutdown -machine q35 -hda $(IMG)

all: build

build:
	mkdir -p $(BUILDDIR) && cd $(BUILDDIR) && meson setup .. && ninja

kvm: build
	$(EMU) $(KVM) $(EFLAGS)

kvm-efi: build
	$(EMU) -bios /usr/share/qemu/OVMF.fd $(KVM) $(EFLAGS)

run: build
	$(EMU) $(EFLAGS)

run-efi: build
	$(EMU) -bios /usr/share/qemu/OVMF.fd $(EFLAGS)

.PHONY : clean build run
clean:
	rm -f $(KERNEL)
	rm -f $(IMG)
	rm -rf $(BUILDDIR)
