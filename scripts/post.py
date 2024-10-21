#!/usr/bin/python3

import argparse
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()

parser.add_argument("name", help = "Project name")
parser.add_argument("-s", "--source", help = "The source meson folder for the project.", required = True)
parser.add_argument("-r", "--sys", help = "The sysroot folder.", required = True)
parser.add_argument("-b", "--build", help = "The meson build folder.", required = True)
parser.add_argument("-k", "--kernel", help = "Kernel ELF file path.", required = True)
args = parser.parse_args()

source_dir = Path(args.source).resolve()
build_dir = Path(args.build).resolve()
sysroot_dir = Path(args.sys).resolve()
mount_dir = Path("/mnt/loop", f"{args.name}").resolve()

kernel_path = Path(args.kernel).resolve()
image_path = Path(args.build, f"{args.name}.img").resolve()
vmdk_path = Path(args.build, f"{args.name}.vmdk").resolve()

def do_parts():
    image_path.touch(exist_ok = True)

    subprocess.run(["dd", "if=/dev/zero", "bs=1G", "count=2", f"of={str(image_path)}"])

    subprocess.run(["parted", "-s", str(image_path), "mklabel", "msdos"])
    subprocess.run(["parted", "-s", str(image_path), "mkpart", "primary", "1", '50%'])
    subprocess.run(["parted", "-s", str(image_path), "mkpart", "primary", "50%", "100%"])

def do_echfs():
    subprocess.run(["echfs-utils", "-m", "-p0", str(image_path), "quick-format", "512"])
    subprocess.run(["echfs-utils", "-m", "-p0", str(image_path), "import", str(Path(source_dir, "misc", "limine.cfg").resolve()), "limine.cfg"])
    subprocess.run(["echfs-utils", "-m", "-p0", str(image_path), "import", str(Path(source_dir, "misc", "limine.sys").resolve()), "limine.sys"])

    """
    TODO:

        if [[ -d ${MESON_SOURCE_DIR}/init && ! -L ${MESON_SOURCE_DIR}/init ]]; then
            ls ${MESON_SOURCE_DIR}/init | cpio -ov > ${MESON_BUILD_DIR}/initrd
        fi
    """

    subprocess.run(["echfs-utils", "-m", "-p0", str(image_path), "import", str(kernel_path), kernel_path.name])

def do_losetup():
    subprocess.run(["fdisk", str(image_path)], input = """
t
2
ef

w
q
""", text = True)    
    part_offset = subprocess.check_output([f"parted -s {str(image_path)} unit s print | sed 's/^ //g' | grep \"^2 \" | tr -s ' ' | cut -d ' ' -f2 |  sed 's/s//g'"], shell = True, text = True).strip()
    subprocess.run(["mkfs.fat", "-F", "32", "--offset", part_offset, str(image_path)])

    free_device = subprocess.check_output(["losetup", "-f"], text = True).strip()
    subprocess.run(["sudo", "losetup", "--partscan", free_device, str(image_path)])

    mount_dir.mkdir(parents = True, exist_ok = True)
    subprocess.run(["sudo", "mount", "-o", "uid=1000,gid=1000", f"{free_device}p2", str(mount_dir)])

    return free_device

def do_efi():
    """
    sudo mkdir -p ${MESON_MOUNT_DIR}/EFI/BOOT
    sudo cp ${MESON_KERNEL} ${MESON_MOUNT_DIR}
    sudo cp ${MESON_SOURCE_DIR}/misc/limine.efi ${MESON_MOUNT_DIR}/EFI/BOOT/BOOTX64.EFI
    """
    efi_dir = Path(mount_dir, "EFI", "BOOT").resolve()
    efi_dir.mkdir(parents = True, exist_ok = True)

    loader_file = Path(source_dir, "misc", "limine.efi").resolve()

    subprocess.run(["cp", str(loader_file), str(Path(efi_dir, "BOOTX64.EFI").resolve())])
    subprocess.run(["cp", "-r", f"{str(sysroot_dir)}/", f"{str(mount_dir)}/"])

def do_limine_install(device):
    subprocess.run(["sudo", "umount", str(mount_dir)])
    subprocess.run(["sudo", "losetup", "-d", device])

    subprocess.run([Path(source_dir, "misc", "limine-install").resolve(), str(image_path)])

    """
    ${MESON_SOURCE_DIR}/misc/limine-install ${MESON_IMG}
    """

def do_vmdk():
    subprocess.run(["qemu-img", "convert", "-O", "vmdk", str(image_path), str(vmdk_path)])

do_parts()
do_echfs()
device = do_losetup()
do_efi()
do_limine_install(device)
do_vmdk()