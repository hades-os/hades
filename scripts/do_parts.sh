#!/bin/bash

MESON_BUILD_DIR=$1
MESON_IMG=$2

do_target() {
    cd ${MESON_BUILD_DIR}

    dd if=/dev/zero bs=1G count=2 of=${MESON_IMG}
    parted -s ${MESON_IMG} mklabel msdos
    parted -s ${MESON_IMG} mkpart primary 1 50%
    parted -s ${MESON_IMG} mkpart primary 50% 100%
}

do_target