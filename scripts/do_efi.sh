#!/bin/bash

MESON_BUILD_DIR=$1
MESON_SOURCE_DIR=$2
MESON_IMG=$3
MESON_MOUNT_DIR=$4

do_target() {
    cd ${MESON_BUILD_DIR}

    IMG_PART_OFFSET=$(parted -s ${MESON_IMG} unit s print | sed 's/^ //g' | grep "^1 " | tr -s ' ' | cut -d ' ' -f2)
    mkfs.fat -F 32 --offset ${IMG_PART_OFFSET} ${MESON_IMG}
    
    sudo mkdir -p ${MESON_MOUNT_DIR}/EFI/BOOT
    sudo cp ${MESON_BUILD_DIR}/${MESON_KERNEL} ${MESON_MOUNT_DIR}
    sudo cp ${MESON_SOURCE_DIR}/misc/limine.efi ${MESON_MOUNT_DIR}/EFI/BOOT/BOOTX64.EFI
}

do_target