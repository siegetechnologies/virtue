#!/bin/sh
#mount the drive

#mkdir /efs/
#todo TLS using amazon efs mount helper?
#mount -t nfs4 -o nfsvers=4.1,rsize=1048576,wsize=1048576,hard,timeo=600,retrans=2,noresvport fs-7f460835.efs.us-east-1.amazonaws.com:/ /efs/

# Start sshd
chmod 755 /efs/
/usr/sbin/sshd -D
