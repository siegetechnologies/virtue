#!/bin/bash

# TODO ansible template this with the latest version
wget https://github.com/Mirantis/criproxy/releases/download/v0.14.0/criproxy -O /usr/local/bin/criproxy
chmod +x /usr/local/bin/criproxy

cat > /etc/systemd/system/criproxy.service << 'EOF'
[Unit]
Description=CRI Proxy

[Service]
ExecStart=/usr/local/bin/criproxy -logtostderr -connect /var/run/dockershim.sock,virtlet.cloud:/run/virtlet.sock -listen /run/criproxy.sock
Restart=always
StartLimitInterval=0
RestartSec=10

[Install]
WantedBy=kubelet.service
EOF

cat > /etc/systemd/system/dockershim.service << 'EOF'
[Unit]
Description=dockershim for criproxy

[Service]
EnvironmentFile=/etc/sysconfig/kubelet
ExecStart=/usr/local/bin/kubelet --experimental-dockershim --port 11250 "$DAEMON_ARGS"
Restart=always
StartLimitInterval=0
RestartSec=10

[Install]
RequiredBy=criproxy.service
EOF

cat > /lib/systemd/system/kubelet.service << 'EOF'
[Unit]
Description=Kubernetes Kubelet Server
Documentation=https://github.com/kubernetes/kubernetes
After=docker.service

[Service]
EnvironmentFile=/etc/sysconfig/kubelet
ExecStart=/usr/local/bin/kubelet --container-runtime=remote --container-runtime-endpoint=unix:///run/criproxy.sock --image-service-endpoint=unix:///run/criproxy.sock --enable-controller-attach-detach=false "$DAEMON_ARGS"
Restart=always
RestartSec=2s
StartLimitInterval=0
KillMode=pr
EOF

systemctl daemon-reload
systemctl stop kubelet
systemctl enable criproxy dockershim
systemctl start criproxy dockershim kubelet
