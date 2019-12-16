#!/bin/bash

#NODEIP=$(kubectl get nodes --selector=kubernetes.io/role=node -o jsonpath={.items[*].status.addresses[?\(@.type==\"InternalIP\"\)].address})
NODEIP=$(kubectl get nodes --selector=kubernetes.io/role=node -o jsonpath={.items[*].status.addresses[?\(@.type==\"InternalIP\"\)].address})
NODENAME=$(kubectl get nodes -o jsonpath={.items[*].metadata.name} --selector=kubernetes.io/role=node)
ALLIP=$(kubectl get nodes -o jsonpath={.items[*].status.addresses[?\(@.type==\"InternalIP\"\)].address})
#NODENAME=$(kubectl get nodes -o jsonpath={.items[*].metadata.name})

echo We have ${ALLIP} for allip

echo We have ${NODEIP} for nodeip

for k in ${NODEIP}; do
	scp -oStrictHostKeyChecking=no ./install_criproxy.sh ${k}:~/
	ssh -oStrictHostKeyChecking=no ${k} sudo bash ~/install_criproxy.sh
done

echo We have ${NODENAME} for nodename

for j in ${NODENAME}; do
	echo ${j}
	kubectl label node ${j} extraRuntime=virtlet
done

sleep 10

#force restart of dns pods to reload config
kubectl delete pod --namespace=kube-system -l k8s-app=kube-dns
sleep 5

curl https://raw.githubusercontent.com/Mirantis/virtlet/master/deploy/images.yaml >images.yaml
kubectl create configmap -n kube-system virtlet-image-translations --from-file images.yaml
rm images.yaml


sleep 1
#we need to trick bash to re-login here, because ansible hasnt reset the connection since when it added ubuntu to the docker group
#bash -l -c 'docker run --rm mirantis/virtlet:latest virtletctl gen --tag latest | kubectl apply -f -'

#could also probably do... but i dont like it
# This is where we'll put the haven install. We'll have to have his docker image somewhere accessible
sudo docker run --rm mirantis/virtlet:latest virtletctl gen --tag latest | kubectl apply -f -

#sudo one works in practice, bash one does not^^^
