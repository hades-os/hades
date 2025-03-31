#!/bin/bash

img_file=$1
sysroot_dir=$2

echo "Creating root filesystem on ${img_file} with root files dir ${sysroot_dir}"
mke2fs -q -L '' -d $sysroot_dir -r 1 -t ext2 $img_file