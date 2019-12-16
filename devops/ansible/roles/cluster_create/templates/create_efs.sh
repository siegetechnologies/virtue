#!/bin/bash

__CLUSTERNAME={{ virtue_cluster_name }}


#kindof a hack, probably should figure out a more sure-fire way to determine if the cluster is ready
while [[ $(kubectl describe nodes | grep KubeletReady | wc -l) -lt {{node_count + 1}} ]]; do
	echo waiting for kubelets to be ready
	sleep 10
done

echo kublets ready, starting amazon efs creation
sleep 5 #just in case?

#create an EFS
aws efs create-file-system --creation-token pvc-efs | jq ".FileSystemId"

sleep 5
#get the fsid (it might already exist)

__FSID=$(aws efs describe-file-systems --creation-token pvc-efs | jq -r ".FileSystems[].FileSystemId")

#grab vpcid for the cluster


__VPCNAME=$(aws ec2 describe-vpcs --filter "Name=tag:Name,Values=${__CLUSTERNAME}"  | jq '.Vpcs[].VpcId')


#grab subnets for the vpc, filtering away utility ones
__SUBNETIDS=$(aws ec2 describe-subnets --filters "Name=vpc-id,Values=${__VPCNAME}" | jq -r '.Subnets[] | select(contains({Tags: [{Key: "AssociatedNatgateway"} ]}) | not) | .SubnetId')


#grab security groups for the vpc
#TODO trim these down, we dont need all of them

__SECGROUPIDS=$(aws ec2 describe-security-groups --filters "Name=vpc-id,Values=${__VPCNAME}" | jq -r '.SecurityGroups[].GroupId')


SUBNETSTRING=
echo ${__VPCNAME}
for j in ${__SUBNETIDS}; do
	echo ${j}
done
for j in ${__SECGROUPIDS}; do
	echo ${j}
done


echo Creating EFS


#set the EFS mount points

for j in ${__SUBNETIDS}; do
	sleep 1
	echo adding mount target for subnet ${j}
	echo aws efs create-mount-target --file-system-id ${__FSID} --subnet-id ${j} --security-groups ${__SECGROUPIDS[*]}
	aws efs create-mount-target --file-system-id ${__FSID} --subnet-id ${j} --security-groups ${__SECGROUPIDS[*]}
done

#grab mount point IPS														#replace []
#__MIPS=$(aws efs describe-mount-targets --file-system-id ${__FSID} | jq -r '[.MountTargets[].IpAddress + "\\/32"]' | sed -e "s/[][]//g;s/\"/'/g" | tr '\n' ' ')




#results in \"1.1.1.1\/32\" ,
#takes each line, changes them to  """ \"$LINE\/32\", """ (without newline, so they are all together), then removes the last comma
#__MIPS=$(for f in $(aws efs describe-mount-targets --file-system-id ${__FSID} | jq -r '.MountTargets[].IpAddress'); do printf $"'${f}\/32',"; done | tr '\n' ' '|sed -e 's/\(.*\),/\1/')
__MIPS=$( for f in $(aws efs describe-mount-targets --file-system-id ${__FSID} | jq -r '.MountTargets[].IpAddress'); do printf '\"%s\/32\", ' ${f}; done | sed -e 's/\(.*\),/\1/')

echo "${__MIPS}"


sleep 1
#efs should be in creating phase, but its OK to set kube to run it

kubectl create -f template_rbac.yaml

#todo replace this whole hacky sed stuff with a single JQ thing
cat template_efs.json | sed -e "s/__FILESYSTEMID/${__FSID}/g; s/__EFSBLOCKIPS/${__MIPS}/g" | kubectl create -f -
#cat template_efs.json | sed "s/__FILESYSTEMID/${__FSID}/g; s/__EFSBLOCKIPS/${__MIPS}/g"
#cat template_efs.json | jq |sed "s/__FILESYSTEMID/${__FSID}/g"
#cat template_efs.json
#echo "s/__FILESYSTEMID/${__FSID}/g; s/__EFSBLOCKIPS/${__MIPS}/g"

