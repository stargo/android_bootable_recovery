#!/usr/bin/env bash
mkdir -p $OUT/APP
mkdir -p $OUT/install-files/bin/
mkdir -p $OUT/install-files/etc/safestrap/kexec/
mkdir -p $OUT/install-files/etc/safestrap/res/
mkdir -p $OUT/install-files/etc/safestrap/rootfs/
mkdir -p $OUT/recovery/root/etc
mkdir -p $OUT/recovery/root/etc/firmware
mkdir -p $OUT/recovery/root/sbin
cd $ANDROID_BUILD_TOP/bootable/recovery/safestrap/devices/motorola
cp -fr common-omap4/res/* $OUT/install-files/etc/safestrap/res/
cp -fr common-omap4/sbin-blobs/* $OUT/recovery/root/sbin/
cp -fr edison/hijack $OUT/install-files/bin/logwrapper
cp -fr edison/twrp.fstab $OUT/recovery/root/etc/twrp.fstab
cp -fr edison/ss.config $OUT/install-files/etc/safestrap/ss.config
cp -fr edison/ss.config $OUT/APP/ss.config
cp -fr edison/ss.config $OUT/recovery/root/ss.config
cd ../../../gui
