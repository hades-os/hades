#!/bin/bash

MESON_BUILD_DIR=$1
MESON_IMG=$2
MESON_MOUNT_DIR=$3

do_target() {
    cd ${MESON_BUILD_DIR}

    (
        echo t
        echo 2
        echo ef
        echo 
        echo w
        echo q
    ) | fdisk ${MESON_IMG}

    udevil mount ${MESON_IMG} ${MESON_MOUNT_DIR}
}

do_target