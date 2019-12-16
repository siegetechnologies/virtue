#!/bin/sh
#based off of https://github.com/alpinelinux/alpine-make-vm-image/blob/master/example/configure.sh
#if newer than august 29 2017, update please
_step_counter=0
step() {
	_step_counter=$(( _step_counter + 1 ))
	printf '\n\033[1;36m%d) %s\033[0m\n' $_step_counter "$@" >&2  # bold cyan
}


step 'Set up timezone'
setup-timezone -z EST

step 'Set up networking'
cat > /etc/network/interfaces <<-EOF
	iface lo inet loopback
	iface eth0 inet dhcp
EOF
ln -s networking /etc/init.d/net.lo
ln -s networking /etc/init.d/net.eth0

step 'Adjust rc.conf'
sed -Ei \
	-e 's/^[# ](rc_depend_strict)=.*/\1=NO/' \
	-e 's/^[# ](rc_logger)=.*/\1=YES/' \
	-e 's/^[# ](unicode)=.*/\1=YES/' \
	/etc/rc.conf

step 'Enable services'
rc-update add acpid default
rc-update add chronyd default
rc-update add crond default
rc-update add net.eth0 default
rc-update add net.lo boot
rc-update add termencoding boot


#end of alpine example conf

step 'Updating'
apk update
apk upgrade

#step 'Enabling docker and sshd'
#step 'Enabling sshd, docker, and haveged'
step 'Enabling docker and haveged'
rc-update add haveged
rc-update add docker default
#rc-update add sshd
#/etc/init.d/sshd stop
#/etc/init.d/docker stop

step 'installing user'

##TODO change root pw

useradd -m virtuevm -G docker

mkdir -p /home/virtuevm/.ssh/

#todo remove virtuevm auth key stuff
cat > /home/virtuevm/.ssh/authorized_keys <<'EOF'
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDGn5/esmW/GHllMB9Rho1wq/oexmNNeow0/lTIrg9KJbDfKgo0INoC1S1y+5dxQMbSrCuXJkBu9AuQvh9xWsYJ1k5HAuX4aggilhZkyhL+TBqnXXGkyS9ZYiu16RcLx+HGL2kqd/4GLDSWVZ4gIesHYxBcZjmUZ4oPPCTHe0tLHEb/actNGqB+8SRbsFX+/sYfCyNwwJBHsDwwpFkDpvrfYjtGRBBXASmgr+GtT2I2b9CTBVGBHleEs6lflBW4gx8Puf4gDmum22tUGeRiblibkJ6Xjtm9aMZsOo2IRxkzX0cmjjBMc8avydW99Xw/I0yAkhneAmHhZ85Zt4o7wW0j awgaines@awgaines-laptop
EOF
chmod 700 /home/virtuevm/.ssh
chmod 600 /home/virtuevm/.ssh/authorized_keys
chown -R virtuevm:virtuevm /home/virtuevm/.ssh/
chown virtuevm:virtuevm /home/virtuevm/.ssh/authorized_keys
echo virtuevm:$(cat /dev/urandom |tr -dc _A-Z-a-z-0-9 | head -c64) | chpasswd

#todo remove ssh
step 'ssh conf'

cat > /etc/ssh/sshd_config <<'EOF'
	PermitRootLogin no
	PubkeyAuthentication yes
	PasswordAuthentication no
EOF
step 'ssh keygen'

ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa
ssh-keygen -f /etc/ssh/ssh_host_dsa_key -N '' -t dsa
ssh-keygen -f /etc/ssh/ssh_host_ecdsa_key -N '' -t ecdsa -b 521
ssh-keygen -f /etc/ssh/ssh_host_ed25519_key -N '' -t ed25519





step 'extlinux adjust'

#basically sets no delay, sets console to serial

OGAPPEND=$(grep 'APPEND' /boot/extlinux.conf)

cat > /boot/extlinux.conf <<EOF
DEFAULT virt
PROMPT 0
LABEL virt
  SERIAL 0 115200
  LINUX vmlinuz-virt
  INITRD initramfs-virt
  ${OGAPPEND} console=ttyS0,115200
EOF

step 'inittab adjust'
#enables a getty on ttyS0 and disables retundant ttys

sed -i 's/#ttyS0/ttyS0/g; s/tty2/#tty2/g; s/tty3/#tty3/g; s/tty4/#tty4/g; s/tty5/#tty5/g; s/tty6/#tty6/g; s/tty7/#tty7/g' /etc/inittab


step 'make virtue mountpoints'
mkdir -p /mnt/virtueconf/
mkdir -p /mnt/data/
mkdir -p /mnt/urd/
#mkdir -p /mnt/virtueimage/

step 'update fstab'

cat >> /etc/fstab <<'EOF'
LABEL="cidata" /mnt/virtueconf/ iso9660 ro,auto,noatime 0 0
data /mnt/data/ 9p trans=virtio,auto,noatime,rw,access=any 0 0
urd /mnt/urd/ 9p trans=virtio,auto,noatime,rw,access=any 0 0
#virtueimage /mnt/virtueimage/ 9p trans=virtio,auto,noatime,ro,posixacl,loose 0 0
EOF

step 'install virtue config manager'

cat > /etc/init.d/virtueconfig <<'EOF'
#!/sbin/openrc-run
name=$RC_SVCNAME
command="/usr/sbin/virtueconfig"
command_args=""
command_user="root"
command_background="yes"
pidfile="/run/$RC_SVCNAME/$RC_SVCNAME.pid"

depend() {
        need docker
}

start_pre() {
        checkpath --directory --owner $command_user:$command_user --mode 0775 \
                /run/$RC_SVCNAME /var/log/$RC_SVCNAME
}
EOF

cat > /usr/sbin/virtueconfig <<'EOF'
#!/bin/sh
#mkdir -p /mnt/virtueconf/
#until mount -t iso9660 /dev/sr0 /mnt/virtueconf/ -ro
#do
#	sleep 1
#done
#need to loop so that the docker daemon has time to start
until docker load -i /virtueimage.tar
do
	sleep 1
done
#need to loop so that the mount has time to mount
until grep 'VIRTUEID' /mnt/virtueconf/user-data
do
	mount /mnt/virtueconf/
	sleep 1
done
#VIRTUEID=$(grep 'VIRTUEID' /mnt/virtueconf/user-data)
#VIRTUEKEY=$(grep 'VIRTUE_AUTH_SSH' /mnt/virtueconf/user-data)
#echo ${VIRTUEID} > /home/virtuevm/yeet
#echo ${VIRTUEKEY} >> /home/virtuevm/yeet

VIRTUEID=$(grep 'VIRTUEID' /mnt/virtueconf/user-data | cut -d '=' -f 2)
#echo ${VIRTUEID}


grep -e VIRTUEID -e VIRTUE_AUTH_SSH /mnt/virtueconf/user-data > /home/virtuevm/dockerenv

docker run --env-file /home/virtuevm/dockerenv -p 22:22 -h "${VIRTUEID}" --mount "type=bind,src=/mnt/data,dst=/home/docker/data" "$(docker images -q)"
#docker run --env-file /home/virtuevm/dockerenv -p 22:22 $(docker images -q)

EOF

chmod +x /etc/init.d/virtueconfig
chmod +x /usr/sbin/virtueconfig


rc-update add virtueconfig default


#step 'Remove cache'
#this breaks some stuff
#rm -rf /var/cache/*
