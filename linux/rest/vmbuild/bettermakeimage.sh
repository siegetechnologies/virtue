#git clone https://github.com/alpinelinux/alpine-make-vm-image.git
#cd alpine-make-vm-image


#sudo setcap 'cap_sys_admin=ep' $(which qemu-nbd)
sudo rm -f virtuevm.img
for f in /dev/nbd*; do sudo qemu-nbd -d ${f} ; done
#wget https://raw.githubusercontent.com/alpinelinux/alpine-make-vm-image/master/alpine-make-vm-image -O alpine-make-vm-image
#chmod +x alpine-make-vm-image

#sed -i '/set -eu/a PATH="$PATH:/bin/:/sbin/:/usr/bin/:/usr/sbin/"' alpine-make-vm-image


chmod +x alpineconfigure.sh
#todo auto figure out a good size based on docker tar
sudo ./alpine-make-vm-image --image-format qcow2 --image-size 10G --repositories-file alpinerepos --packages "$(cat alpinepackages)" -c virtuevm.img -- ./alpineconfigure.sh
sudo chown $USER virtuevm.img

#wget https://raw.githubusercontent.com/alpinelinux/alpine-make-vm-image/v0.4.0/alpine-make-vm-image \
#    && echo '5fb3270e0d665e51b908e1755b40e9c9156917c0  alpine-make-vm-image' | sha1sum -c \



echo upload to s3?
echo note, this script is deprecated and the next line might not work
aws s3 cp ./virtuevm.img s3://siege-virtuevms/virtuevm.img --acl public-read
