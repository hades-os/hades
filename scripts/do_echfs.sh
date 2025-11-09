#!/bin/bash

MESON_BUILD_DIR=$1
MESON_SOURCE_DIR=$2
MESON_IMG=$3

do_target() {
    cd ${MESON_BUILD_DIR}
    
    echfs-utils -m -p0 ${MESON_IMG} quick-format 512
    echfs-utils -m -p0 ${MESON_IMG} import ${MESON_SOURCE_DIR}/misc/limine.cfg limine.cfg
    echfs-utils -m -p0 ${MESON_IMG} import ${MESON_SOURCE_DIR}/misc/limine.sys limine.sys
}

do_target