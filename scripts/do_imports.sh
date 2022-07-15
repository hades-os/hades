#!/bin/bash

MESON_BUILD_DIR=$1
MESON_SOURCE_DIR=$2

MESON_IMG=$3
MESON_KERNEL=$4

do_initrd() {    
    if [[ -d ${MESON_SOURCE_DIR}/init && ! -L ${MESON_SOURCE_DIR}/init ]]; then
        ls ${MESON_SOURCE_DIR}/init | cpio -ov > ${MESON_BUILD_DIR}/initrd
    fi
}

do_target() {
    do_initrd

    cd ${MESON_BUILD_DIR}
	echfs-utils -m -p0 ${MESON_IMG} import ${MESON_BUILD_DIR}/initrd initrd
	echfs-utils -m -p0 ${MESON_IMG} import ${MESON_BUILD_DIR}/${MESON_KERNEL} ${MESON_KERNEL}
}

do_target