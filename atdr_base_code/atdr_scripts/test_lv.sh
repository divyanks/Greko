echo "Number of disks to perform Automatic testing"
read numOfDisks
echo "Enter size of each disk"
read diskSize
echo "Enter lvm group name"
read grupName
echo "Group name is: $grupName"

cmdExecution() {
diskSize=1
serverIp="127.0.0.1"
pid=$1
srcDisk=$2
trgtDisk=$3

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

t=$(($pid + 1))
ebdrDev="/dev/ebdr""$t"
mkfs.ext3 $ebdrDev
mntDir="/mnt""$pid"
mkdir $mntDir
mount $ebdrDev $mntDir
cp -r ../utils  $mntDir 
./ebdradm --snap $srcDisk $pid
./ebdradm --metadata $pid
# ./ebdradm --resync

}

for ((i=0; i<$numOfDisks; i++))
do
cmd="lvcreate -L"" $diskSize""G -n lv0""$i $grupName"
$cmd
lvName="lv""0""$i"
device=$(ls -lt /dev/mapper|  grep $lvName | awk '{print $(NF)}' | awk -F"/" '{print $NF}') 
echo "$device"
dev="/dev/""$device"
# echo $cmd
# echo $dev
if [ $((i % 2)) -eq 0 ]; then
  echo "$i It is source: " $dev
  disks[$i]=$dev
else
  echo "$i It is target: " $dev
  disks[$i]=$dev
fi
done


for ((i=0; i<$numOfDisks; i++))
do
  echo "$i source is: ${disks[$i]}"
  echo "${i+1} target is: ${disks[$i+1]}"
t=$((i / 2))
  cmdExecution $t ${disks[$i]} ${disks[$i+1]}
  i=$(($i + 1))
done
