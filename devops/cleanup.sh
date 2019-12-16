#/bin/bash

__ASGNAMES=$(aws autoscaling describe-auto-scaling-groups | jq -r ."AutoScalingGroups[].AutoScalingGroupName")
for f in ${__ASGNAMES}; do
	echo Deleting OSG ${f}
	aws autoscaling delete-auto-scaling-group --auto-scaling-group-name ${f} --force-delete #force means it wont wait for the instances to terminate
done

__LAUNCHCFGNAMES=$(aws autoscaling describe-launch-configurations | jq -r ."LaunchConfigurations[].LaunchConfigurationName")
for f in ${__LAUNCHCFGNAMES}; do
	echo Deleting LAUNCHCFG ${f}
	aws autoscaling delete-launch-configuration --launch-configuration-name ${f}
done

__DIRECTORIES=$(aws ds describe-directories | jq -r ".DirectoryDescriptions[] | .DirectoryId")
for f in ${__DIRECTORIES}; do
	echo Deleting Directory ${f}
	# commented out for APL account
	aws ds delete-directory --directory-id ${f}
done



#todo ignore terminated ones
__INSTANCES=$(aws ec2 describe-instances | jq -r ".Reservations[].Instances[] | select(.State.Name != \"terminated\") | .InstanceId")
for f in ${__INSTANCES}; do
	echo Terminating INSTANCE ${f}
	# commented out for APL account
	aws ec2 terminate-instances --instance-ids ${f}
done

echo "Waiting for instances to terminate"
while [[ $(aws ec2 describe-instances | jq -r ".Reservations[].Instances[] | select(.State.Name != \"terminated\") | .InstanceId") ]]; do
	sleep 1
done


__FILESYSTEMS=$(aws efs describe-file-systems | jq -r ".FileSystems[].FileSystemId")
for f in ${__FILESYSTEMS}; do
	__MOUNTIES=$(aws efs describe-mount-targets --file-system-id ${f} | jq -r ".MountTargets[].MountTargetId")
	for m in ${__MOUNTIES}; do
		echo Deleting mount target ${m}
		aws efs delete-mount-target --mount-target-id ${m}
	done
done
#wait for mounties to be deleted, and then delete the fs
for f in ${__FILESYSTEMS}; do
	__MOUNTIES=$(aws efs describe-mount-targets --file-system-id ${f} | jq -r ".MountTargets[].MountTargetId")
	while [[ ! -z ${__MOUNTIES} ]]; do
		echo "Waiting for mounties for ${f} to be deleted"
		sleep 1
		__MOUNTIES=$(aws efs describe-mount-targets --file-system-id ${f} | jq -r ".MountTargets[].MountTargetId")
	done

	echo Deleting FS ${f}
	aws efs delete-file-system --file-system-id ${f}
done


__BUCKETS=$(aws s3api list-buckets | jq -r ".Buckets[].Name")
for f in ${__BUCKETS}; do
	aws s3api delete-objects --bucket ${f} --delete "$(aws s3api list-object-versions --bucket ${f} | jq '{Objects: [.Versions[] | {Key:.Key, VersionId : .VersionId}], Quiet: false}')"
	aws s3api delete-bucket --bucket ${f}
done

__ZONES=$(aws route53 list-hosted-zones | jq -r ".HostedZones[].Id")
for f in ${__ZONES}; do
	echo Deleting record sets in hosted zone ${f}
#	echo TODO OTODO OTODOD TODODODODO
#	__RS=$(aws route53 list-resource-record-sets --hosted-zone-id ${f} | jq -r ".ResourceRecordSets[] | select( .Type == \"A\" ) | .Name")
#	for r in ${__RS}; do
#		echo ${r}
#		aws route53 change-resource-record-sets --hosted-zone-id ${f} --change-batch "{\"Changes\":[{\"Action\":\"DELETE\",\"ResourceRecordSet\":{\"Type\":\"A\",\"Name\":\"${r}\"}}]}"
#	done
	__RS=$(aws route53 list-resource-record-sets --hosted-zone-id ${f} | jq -c ".ResourceRecordSets[] | select( .Type ==\"A\")")
	for q in ${__RS}; do
		aws route53 change-resource-record-sets --hosted-zone-id ${f} --change-batch "{\"Changes\":[{\"Action\":\"DELETE\",\"ResourceRecordSet\":${q}}]}"
	done

done


for f in ${__ZONES}; do
	echo Deleting hosted zone ${f}
	aws route53 delete-hosted-zone --id ${f}
done

__REPOS=$(aws ecr describe-repositories | jq -r ".repositories[].repositoryName")
for f in ${__REPOS}; do
	echo Deleting repo ${f}
	aws ecr delete-repository --repository-name ${f} --force
done




#todo workspace directories



__LB=$(aws elb describe-load-balancers | jq -r ".LoadBalancerDescriptions[].LoadBalancerName")
for f in ${__LB}; do
	echo Deleting LB ${f}
	aws elb delete-load-balancer --load-balancer-name ${f}
done


__VOLUMES=$(aws ec2 describe-volumes | jq -r ".Volumes[].VolumeId")
for f in ${__VOLUMES}; do
	echo Deleting volume ${f}
	aws ec2 delete-volume --volume-id ${f}
done

__KP=$(aws ec2 describe-key-pairs | jq -r ".KeyPairs[].KeyName | select(contains(\"siege-virtuecluster.com\"))")
for f in ${__KP}; do
	echo Deleting KP ${f}
	aws ec2 delete-key-pair --key-name ${f}
done


#todo subnets
#todo Security groups
#todo Network ACLS
#todo VPN stuff
#todo route tables
#todo network interfaces

echo "Removing Internet gateways"
__IGWS=$(aws ec2 describe-internet-gateways | jq -r ".InternetGateways[].InternetGatewayId" )
for f in ${__IGWS}; do
	echo Deleting internet gateway ${f}
	__IGWVPCS=$(aws ec2 describe-internet-gateways --filters "Name=internet-gateway-id,Values=[${f}]" | jq -r ".InternetGateways[].Attachments[].VpcId" )
	for i in ${__IGWVPCS}; do
		aws ec2 detach-internet-gateway --internet-gateway-id ${f} --vpc-id ${i}
	done
	aws ec2 delete-internet-gateway --internet-gateway-id ${f}
done

#will have to do the sleep thing
echo "Removing VPC Peering connections"
__PFXEN=$(aws ec2 describe-vpc-peering-connections | jq -r ".VpcPeeringConnections[].VpcPeeringConnectionId")
for f in ${__PFXEN}; do
	echo Deleting vpc peering connection ${f}
	aws ec2 delete-vpc-peering-connection --vpc-peering-connection-id ${f}
done

#todo nat gateways?
echo "Removing NAT gateways"
__NAT_GATEWAYS=$(aws ec2 describe-nat-gateways | jq -r ".NatGateways[].NatGatewayId" )
for f in ${__NAT_GATEWAYS}; do
	echo "Delete gateway ${f}"
	aws ec2 delete-nat-gateway --nat-gateway-id ${f}
done


echo THis might error....
__ELASTICIPS=$(aws ec2 describe-addresses | jq -r '.Addresses[] | select(.AllocationId != "eipalloc-02cd3138960db2ab3" ) | .AllocationId')
for f in ${__ELASTICIPS}; do
	echo Release elastic ips${f}
	aws ec2 release-address --allocation-id ${f}
done

__ELASTICIPS=$(aws ec2 describe-addresses | jq -r '.Addresses[] | select(.AllocationId != "eipalloc-02cd3138960db2ab3" ) | .AssocationId')
for f in ${__ELASTICIPS}; do
	echo Disass elastic ips${f}
	aws ec2 disassociate-address --association-id ${f}
done
for f in ${__ELASTICIPS}; do
	echo Release elastic ips${f}
	aws ec2 release-address --allociation-id ${f}
done

__ELASTICIPS=$(aws ec2 describe-addresses | jq -r '.Addresses[] | select(.AllocationId != "eipalloc-02cd3138960db2ab3" ) | .AllocationId')
for f in ${__ELASTICIPS}; do
	echo Disass elastic ips${f}
	aws ec2 disassociate-address --allocation-id ${f}
done
for f in ${__ELASTICIPS}; do
	echo Release elastic ips${f}
	aws ec2 release-address --allocation-id ${f}
done

__ELASTICIPS=$(aws ec2 describe-addresses | jq -r '.Addresses[] | select(.AllocationId != "eipalloc-02cd3138960db2ab3" ) | .PublicIp')
for f in ${__ELASTICIPS}; do
	echo Disass elastic ips${f}
	aws ec2 disassociate-address --public-ip ${f}
done
for f in ${__ELASTICIPS}; do
	echo Release elastic ips${f}
	aws ec2 release-address --public-ip ${f}
done

echo "Deleting subnets"
__SUBNETS=$(aws ec2 describe-subnets | jq -r ".Subnets[].SubnetId" )
for f in ${__SUBNETS}; do
	echo "Deleting ${f}"
	aws ec2 delete-subnet --subnet-id ${f}
done

echo end of this might error

# only can delete route tables with no associations
# everything else will be a main table
__RTS=$(aws ec2 describe-route-tables | jq -r ".RouteTables[] | select( .Associations==[] ) | .RouteTableId")
echo "Deleting Route tables"
for f in ${__RTS}; do
	echo Deleting route table ${f}
	aws ec2 delete-route-table --route-table-id ${f}
done

__SGS=$(aws ec2 describe-security-groups | jq -r ".SecurityGroups[] | select( .GroupName != \"default\" ) | .GroupId" )
for f in ${__SGS}; do
	echo Deleting security group ${f}
	aws ec2 delete-security-group --group-id ${f}
done


__DHCPOPTS=$(aws ec2 describe-dhcp-options | jq -r ".DhcpOptions[] | select(contains({Tags:[{Value:\"siege-virtuecluster.com\"}]})) | .DhcpOptionsId")
for f in ${__DHCPOPTS}; do
	echo Deleting dhcptopts ${f}
	aws ec2 delete-dhcp-options --dhcp-options-id ${f}
done


#might need to run a few times
__VPCS=$(aws ec2 describe-vpcs | jq -r ".Vpcs[] | select(.IsDefault == false) | .VpcId")
for f in ${__VPCS}; do
	echo Deleting vpc ${f}
	aws ec2 delete-vpc --vpc-id ${f}
done


#__=$()
#for f in ${__}; do
#	echo Deleting ${f}
#done
