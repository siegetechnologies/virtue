#!/bin/bash
sed -i 's/DAEMON_ARGS\="/DAEMON_ARGS\="--resolv-conf=\/run\/systemd\/resolve\/resolv.conf\ /g' /etc/sysconfig/kubelet
systemctl restart kubelet
