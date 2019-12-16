#!/bin/bash

#NODEIP=$(kubectl get nodes --selector=kubernetes.io/role=node -o jsonpath={.items[*].status.addresses[?\(@.type==\"InternalIP\"\)].address})
NODEIP=$(kubectl get nodes --selector=kubernetes.io/role=node -o jsonpath={.items[*].status.addresses[?\(@.type==\"InternalIP\"\)].address})
NODENAME=$(kubectl get nodes -o jsonpath={.items[*].metadata.name} --selector=kubernetes.io/role=node)
ALLIP=$(kubectl get nodes -o jsonpath={.items[*].status.addresses[?\(@.type==\"InternalIP\"\)].address})
#NODENAME=$(kubectl get nodes -o jsonpath={.items[*].metadata.name})

echo We have ${ALLIP} for allip

echo We have ${NODEIP} for nodeip

for b in ${ALLIP}; do
	scp -oStrictHostKeyChecking=no ./fix_dns_helper.sh ${b}:~/
	ssh -oStrictHostKeyChecking=no ${b} sudo bash ~/fix_dns_helper.sh
done

sleep 10

#force restart of dns pods to reload config
kubectl delete pod --namespace=kube-system -l k8s-app=kube-dns
