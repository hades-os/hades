#!/bin/bash

MESON_SOURCE_DIR=$1
MESON_IMG=$2
MESON_MOUNT_DIR=$3

do_target() {
	udevil umount ${MESON_MOUNT_DIR}
	${MESON_SOURCE_DIR}/misc/limine-install ${MESON_IMG}
}

do_target