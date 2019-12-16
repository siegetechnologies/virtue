#!/bin/bash

__CLUSTERNAME={{virtue_cluster_name}}
__S3STATEURL=s3://{{virtue_cluster_name}}
__KUBE_IMAGE=ami-0ac019f4fcb7cb7e6 # 18.04
#__KUBE_IMAGE=ami-759bc50a # 16.04
# Create SSH Key for
[ ! -f ~/.ssh/id_rsa ] && ssh-keygen -f ~/.ssh/id_rsa -t rsa -N ''

__HOSTED_ZONE=$(aws route53 list-hosted-zones | jq .[] | jq '.[] | select(.Name=="${__CLUSTERNAME}.") | .Id' | cut -d '"' -f2 | cut -d '/' -f3)

sleep 1
# Create the Kubernetes cluster
echo "### BUILDING CLUSTER ###"
kops create cluster ${__CLUSTERNAME} \
--state ${__S3STATEURL} \
--node-count "{{node_count}}" \
--master-count 1 \
--node-size i3.metal \
--node-volume-size 4096 \
--master-size t2.medium \
--zones "{{ virtue_aws_region }}a,{{ virtue_aws_region }}b,{{ virtue_aws_region }}c" \
--master-zones "{{ virtue_aws_region }}a" \
--networking calico \
--dns private \
--image ${__KUBE_IMAGE} \
--topology private \
--api-loadbalancer-type internal \
--dns-zone="${__HOSTEDZONE}" \
--network-cidr "{{cluster_cidr}}" \
--yes
