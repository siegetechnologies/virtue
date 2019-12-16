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


#TODO if you want to re-enable ssh, you must add openssh to alpine-packages
#mkdir -p /home/virtuevm/.ssh/

#todo remove virtuevm auth key stuff
#cat > /home/virtuevm/.ssh/authorized_keys <<'EOF'
#ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDGn5/esmW/GHllMB9Rho1wq/oexmNNeow0/lTIrg9KJbDfKgo0INoC1S1y+5dxQMbSrCuXJkBu9AuQvh9xWsYJ1k5HAuX4aggilhZkyhL+TBqnXXGkyS9ZYiu16RcLx+HGL2kqd/4GLDSWVZ4gIesHYxBcZjmUZ4oPPCTHe0tLHEb/actNGqB+8SRbsFX+/sYfCyNwwJBHsDwwpFkDpvrfYjtGRBBXASmgr+GtT2I2b9CTBVGBHleEs6lflBW4gx8Puf4gDmum22tUGeRiblibkJ6Xjtm9aMZsOo2IRxkzX0cmjjBMc8avydW99Xw/I0yAkhneAmHhZ85Zt4o7wW0j awgaines@awgaines-laptop
#EOF
#chmod 700 /home/virtuevm/.ssh
#chmod 600 /home/virtuevm/.ssh/authorized_keys
#chown -R virtuevm:virtuevm /home/virtuevm/.ssh/
#chown virtuevm:virtuevm /home/virtuevm/.ssh/authorized_keys
#echo virtuevm:$(cat /dev/urandom |tr -dc _A-Z-a-z-0-9 | head -c64) | chpasswd

#step 'ssh conf'

#cat > /etc/ssh/sshd_config <<'EOF'
#	PermitRootLogin no
#	PubkeyAuthentication yes
#	PasswordAuthentication no
#EOF
#step 'ssh keygen'

#ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa
#ssh-keygen -f /etc/ssh/ssh_host_dsa_key -N '' -t dsa
#ssh-keygen -f /etc/ssh/ssh_host_ecdsa_key -N '' -t ecdsa -b 521
#ssh-keygen -f /etc/ssh/ssh_host_ed25519_key -N '' -t ed25519





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
#data /mnt/data/ 9p trans=virtio,auto,noatime,rw,access=any 0 0
#urd /mnt/urd/ 9p trans=virtio,auto,noatime,rw,access=any 0 0
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
#start fluentd
fluentd -d /var/run/maven_fluentd.pid

#shuffle maven
#todo move this to its own service that starts earlier? (No dependence on docker)
/usr/sbin/mavenshuffle
#need to loop so that the docker daemon has time to start
if [ -f /virtueimage.tar ]; then
	until docker load -i /virtueimage.tar
	do
		sleep 1
	done
else
	until docker info
	do
		sleep 1
	done
fi



#need to loop so that the mount has time to mount
until grep 'VIRTUEID' /mnt/virtueconf/user-data
do
	mount /mnt/virtueconf/
	sleep 1
done
VIRTUEID=$(grep 'VIRTUEID' /mnt/virtueconf/user-data | cut -d '=' -f 2-)

if [ "$VIRTUEID" == "BAKE" ]; then
	echo "Shake n bake!"
	#docker should be done here
#	/etc/init.d/docker stop
	rm /virtueimage.tar
	sync
	fstrim -v /
	sync
	poweroff
fi



#now that we know the virtueid, we can fully load maven
modprobe mod_log virtueid_in=${VIRTUEID}
modprobe mod_sensors
modprobe mod_net
#modprobe mod_syscall

#delete loaded modules so they can not be looked at
MAVEN_DIR=/lib/modules/$(uname -r)/extra
find ${MAVEN_DIR} -name '*.ko' -delete

#lets configure maven then
VIRTUE_MAVENCONF=$(grep 'VIRTUE_MAVENCONF' /mnt/virtueconf/user-data | cut -d '=' -f 2- | base64 -d)

#untested
echo "${VIRTUE_MAVENCONF}" | while read x; do
timeout -t 1 maven_us "${x}"
done

timeout -t 1 maven_us "stop_listener"


grep -e VIRTUEID -e VIRTUE_AUTH_SSH /mnt/virtueconf/user-data > /home/virtuevm/dockerenv

PSAN=$(grep 'VIRTUE_PERSISTENT_AUTH_NAME' /mnt/virtueconf/user-data | cut -d '=' -f 2-)
USAN=$(grep 'VIRTUE_USER_AUTH_NAME' /mnt/virtueconf/user-data | cut -d '=' -f 2-)


MOUNT_OPS=""

#mount em!
if [[ ! -z ${PSAN} ]]; then
	mkdir -p /mnt/urd/
	echo mounting psan "${PSAN}"
	modprobe fuse
	grep 'VIRTUE_PERSISTENT_AUTH_KEY=' /mnt/virtueconf/user-data | cut -d '=' -f 2- | base64 -d >/root/psak.pem
	chmod 400 /root/psak.pem
	sshfs -p 2222 -o StrictHostKeyChecking=no,allow_other,IdentityFile=/root/psak.pem "${PSAN}@10.1.1.101:/efs/${PSAN}" /mnt/urd/
	MOUNT_OPS="${MOUNT_OPS} --mount type=bind,src=/mnt/urd,dst=/home/docker/"
fi
if [[ ! -z ${USAN} ]]; then
	mkdir -p /mnt/data/
	echo mounting usan "${USAN}"
	modprobe fuse
	grep 'VIRTUE_USER_AUTH_KEY=' /mnt/virtueconf/user-data | cut -d '=' -f 2- | base64 -d >/root/usak.pem
	chmod 400 /root/usak.pem
	sshfs -p 2222 -o StrictHostKeyChecking=no,allow_other,IdentityFile=/root/usak.pem "${USAN}@10.1.1.101:/efs/${USAN}" /mnt/data/
	MOUNT_OPS="${MOUNT_OPS} --mount type=bind,src=/mnt/data,dst=/home/docker/data/"
fi

docker run -d --env-file /home/virtuevm/dockerenv -p 22:22 -h "${VIRTUEID}" ${MOUNT_OPS} "$(docker images -q)"

EOF

chmod +x /etc/init.d/virtueconfig
chmod +x /usr/sbin/virtueconfig


rc-update add virtueconfig default



#todo put this in a bind mount that is accessible to vmorig?
#it would be faster maybe, idk
#it might also result in a smaller image, as ext4 doesnt remove data efficently with qcow2

step 'git r2'
cd /home/virtuevm
git clone --depth=1 https://github.com/radare/radare2
step 'compile r2'
cd /home/virtuevm/radare2/
./sys/install.sh
cd /home/virtuevm/radare2/libr/
make -j 8 && make install
step ' test r2'
r2 -v

step 'Remove r2 sources'
rm -rf /home/virtuevm/radare2/

step 'Compile maven'
cd /maven/core/
make clean && make -j8 && make modules_install && depmod -a $(ls /lib/modules|head -n1)

step 'Removing maven sources'
rm -rf /maven/


step 'Compile maven_us'
cd /maven_us/
make clean && make -j8 && make install

step 'Removing maven_us sources'
rm -rf /maven_us/

step 'Compiling mangler'
cd /mangler/
make clean && make -j8 && make install

step 'Testing mangler'
cd /mangler/elftest/
make clean && ./fasttest.sh
cd /mangler/elftest/roboarchng-test/
make clean && ./fasttest.sh

step 'Removing mangler sources'
rm -rf /mangler/


step 'Installing fluentd'
cd /home/virtuevm
gem install fluentd
gem install fluent-plugin-secure-forward

step 'Remove unneeded packages'
apk del git make alpine-sdk diffutils linux-virt-dev ruby-dev ruby-bundler linux-headers
##todo?


step 'Moving modules to backs'
MAVEN_DIR=/lib/modules/$(ls /lib/modules|head -n1)/extra
entries=$(find ${MAVEN_DIR} -name '*.ko')
for ent in ${entries}; do
	mv ${ent} ${ent}.orig
done


step 'install shuffle scripts'
cat > /usr/sbin/mavenshuffle <<'EOF'
#!/bin/bash
MAVEN_DIR=/lib/modules/$(uname -r)/extra
entries=$(find ${MAVEN_DIR} -name '*.ko.orig')
for entry in ${entries} ; do
	final=${entry%%.orig}
	elf-mangler -sbCM ${entry} ${final}
#	cp ${entry} ${final}	#elf-mangler currently has an issue with one of the modules?
done
EOF

chmod +x /usr/sbin/mavenshuffle


#step 'Cleanup APK cache'
#apk cache clean
#apk cache -v sync
#step 'Remove cache'
#this breaks some stuff
#rm -rf /var/cache/*

#cat /sys/block/*/queue/discard_*

step 'fstrim'
fstrim -v /
