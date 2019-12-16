#!/bin/bash
wget http://cloud-images.ubuntu.com/bionic/current/bionic-server-cloudimg-amd64.img -O virtuevm.img
virt-customize -a virtuevm.img --commands-from-file virtuevm.create
#virt-builder ubuntu-18.04  --commands-from-file virtuevm.create -o virtuevm.qcow2 --format qcow2 --size 20G
