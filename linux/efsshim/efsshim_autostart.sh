#!/bin/bash

# Build docker container
sudo docker build /home/ubuntu -t efscontainer

# wait for efs to be created
__FS=$(aws efs describe-file-systems --creation-token testefs | jq -r ".FileSystems[0].FileSystemId")
while [[ ${__FS} == "" ]]
do
	sleep 1s
	__FS=$(aws efs describe-file-systems --creation-token pvc-efs | jq -r ".FileSystems[0].FileSystemId")
done

echo "Got file system ${__FS}"

__MOUNT_IP=$(aws efs describe-mount-targets --file-system-id ${__FS} | jq -r ".MountTargets[0].IpAddress")

echo "Mount ip is ${__MOUNT_IP}"


echo sudo mount -t nfs4 -o nfsvers=4.1,rsize=1048576,wsize=1048576,hard,timeo=600,retrans=2,owner, ${__MOUNT_IP}:/ /home/ubuntu/efsmount
# mount efs
sudo mount -t nfs4 -o nfsvers=4.1,rsize=1048576,wsize=1048576,hard,timeo=600,retrans=2,owner, ${__MOUNT_IP}:/ /home/ubuntu/efsmount
sudo chown ubuntu:ubuntu efsmount
sudo chmod 755 efsmount #permissions needs to have execute in order to CD to a dir

# start docker container
echo "Starting docker container"
sudo docker run -d -p 2222:22 -v /home/ubuntu/efsmount:/efs/ efscontainer
