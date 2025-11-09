#!/bin/sh
rm -f limine.efi
rm -f limine.sys
rm -f limine-install

ver="2.0-branch"

wget https://github.com/limine-bootloader/limine/blob/v$ver-binary/limine-install-linux-x86_64?raw=true -O limine-install
chmod +x limine-install

wget https://github.com/limine-bootloader/limine/blob/v$ver-binary/BOOTX64.EFI?raw=true -O limine.efi
wget https://github.com/limine-bootloader/limine/blob/v$ver-binary/limine.sys?raw=true -O limine.sys