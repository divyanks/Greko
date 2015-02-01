#!/bin/sh

../utils/stop_ebdrd
echo "Number of disks to perform Automatic testing"
read numOfDisks

for ((i=0; i<$numOfDisks; i++))
do
mntDir="/mnt""$i"
umount $mntDir
lvName="lv""0""$i"
lvrm="lvremove /dev/vg01/""$lvName"
done

umount /mnt
sleep 2
rmmod ebdr
