#!/bin/sh

echo "Enter [Source disk]: "
read srcDisk
echo "Enter [Target disk]: "
read trgtDisk
echo "Enter [Disk Size]: "
read diskSize
echo "Enter [Server Ip]: "
read serverIp
echo "Enter [pid]: "
read pid
echo "srcDisk = " $srcDisk "trget Disk= " $trgtDisk "disk Size = " $diskSize

bitmapInBytes=16384
bitmapInSectors=$(($bitmapInBytes/512))
grainSize=$(($diskSize * 1024 * 1024 * 1024))
bitmapSize=$((8*16384))
grainSize=$(($grainSize/$bitmapSize))
grainInSectors=$(($grainSize/512))
chunkSize=8
echo " grain size =" $grainSize
echo " grain size in sectors =" $grainInSectors

cat /dev/null > /var/log/syslog
dmesg -c
make clean
make

./ebdrds
sleep 5
./ebdrdc
cd ../utils/
 ./ebdradm --create $srcDisk $grainSize $bitmapInBytes
./ebdradm --mkpartner $serverIp 50 $srcDisk $bitmapInBytes $grainInSectors $chunkSize
./ebdradm --mkrelationpeer $pid 1 $trgtDisk $bitmapInBytes $grainInSectors $chunkSize
./ebdradm --listps
./ebdradm --listpc
./ebdradm --listrs
./ebdradm --listrc
mkfs.ext3 /dev/ebdr1
mount /dev/ebdr1 /mnt
cp -r ../utils  /mnt -r
./ebdradm --snap $srcDisk $pid
./ebdradm --metadata $pid
# ./ebdradm --resync

