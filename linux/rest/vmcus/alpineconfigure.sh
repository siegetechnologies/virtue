#!/bin/sh
#based off of https://github.com/alpinelinux/alpine-make-vm-image/blob/master/example/configure.sh
#if newer than august 29 2017, update please
_step_counter=0
step() {
	_step_counter=$(( _step_counter + 1 ))
	printf '\n\033[1;36m%d) %s\033[0m\n' $_step_counter "$@" >&2  # bold cyan
}

echo virtuevm:$(cat /dev/urandom |tr -dc _A-Z-a-z-0-9 | head -c64) | chpasswd

#todo remove ssh
#step 'ssh conf'

#cat > /etc/ssh/sshd_config <<'EOF'
#	PermitRootLogin no
#	PubkeyAuthentication yes
#	PasswordAuthentication no
#EOF
#step 'ssh keygen'

#rm /etc/ssh/ssh_host_rsa_key /etc/ssh/ssh_host_dsa_key /etc/ssh/ssh_host_ecdsa_key /etc/ssh/ssh_host_ed25519_key -f


#yes -y |ssh-keygen -f /etc/ssh/ssh_host_rsa_key -N '' -t rsa
#yes -y |ssh-keygen -f /etc/ssh/ssh_host_dsa_key -N '' -t dsa
#yes -y |ssh-keygen -f /etc/ssh/ssh_host_ecdsa_key -N '' -t ecdsa -b 521
#yes -y |ssh-keygen -f /etc/ssh/ssh_host_ed25519_key -N '' -t ed25519



step 'trim'
fstrim -v /
