FROM ubuntu

ENV DOCKERFILE_UPDATED 2018-10-14
ENV DEBIAN_FRONTEND noninteractive

RUN (apt-get update && \
     apt-get install -y software-properties-common && \
     apt-get update && \
     apt-get install -y openssh-server nfs-common sudo)

RUN (mkdir -p /var/run/sshd && \
     mkdir -p /etc/docker/ssh && \
     mkdir -p /var/ssh/ && \
#     rm /etc/skel/* && \
     sed -ri 's/AuthorizedKeysFile .ssh/authorized_keys/#AuthorizedKeysFile .ssh/authorized_keys/g' /etc/ssh/sshd_config && \
     echo "AuthorizedKeysFile /var/ssh/%u/authorized_keys" >> /etc/ssh/sshd_config && \
     sed -ri 's/UseDNS yes/#UseDNS yes/g' /etc/ssh/sshd_config && \
     echo "UseDNS no" >> /etc/ssh/sshd_config && \
     sed -ri 's/X11Forwarding yes/#X11Forwarding yes/g' /etc/ssh/sshd_config && \
     echo "X11Forwarding no" >> /etc/ssh/sshd_config && \
     sed -ri 's/AllowTcpForwarding yes/#AllowTcpForwarding yes/g' /etc/ssh/sshd_config && \
     echo "AllowTcpForwarding no" >> /etc/ssh/sshd_config)


#todo group and IPTABLES for not allowwing any networks

# Create a user
#RUN (useradd -m efsshim && usermod -s /sbin/nologin efsshim && mkdir /home/efsshim/.ssh/)
RUN (useradd -m efsshim && mkdir /var/ssh/efsshim/)
ADD id_rsa.pub /var/ssh/efsshim/authorized_keys
RUN (chown -R efsshim:efsshim /var/ssh/efsshim/ &&\
	chmod 700 /var/ssh/efsshim/ &&\
	chmod 600 /var/ssh/efsshim/authorized_keys )


RUN (echo "efsshim ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers)

#RUN (groupadd admin)
#RUN (usermod -a -G admin efsshim)
#RUN (usermod -a -G sudo efsshim)	#requires password


# Startup script
ADD start-sshd.sh /root/
ADD create-user.sh /root/
RUN chmod 744 /root/start-sshd.sh
RUN chmod 744 /root/create-user.sh

RUN ln -sf /usr/share/zoneinfo/America/New_York /etc/localtime

EXPOSE 22
ENTRYPOINT ["/root/start-sshd.sh"]
