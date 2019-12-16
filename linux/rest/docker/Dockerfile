FROM ubuntu

ENV DOCKERFILE_UPDATED 2019-2-01
ENV DEBIAN_FRONTEND noninteractive

#RUN (apt-get update && \
#     apt-get install -y software-properties-common && \
#     add-apt-repository -y ppa:x2go/stable && \
#     apt-get update && \
#     apt-get install -y \
#     x2goserver x2goserver-xsession sshfs)

#RUN (mkdir -p /var/run/sshd && \
#     mkdir -p /etc/docker/ssh && \
#     sed -ri 's/UseDNS yes/#UseDNS yes/g' /etc/ssh/sshd_config && \
#     echo "UseDNS no" >> /etc/ssh/sshd_config)

ADD start-sshd.sh /root/

RUN (apt-get update && \
     apt-get install -y software-properties-common && \
     add-apt-repository -y ppa:x2go/stable && \
     apt-get update && \
     apt-get install -y \
     x2goserver x2goserver-xsession sshfs &&\
     mkdir -p /var/run/sshd && \
     mkdir -p /etc/docker/ssh && \
     mkdir -p /var/ssh/ && \
     sed -ri 's/UseDNS yes/#UseDNS yes/g' /etc/ssh/sshd_config && \
     sed -ri 's/AuthorizedKeysFile .ssh/authorized_keys/#AuthorizedKeysFile .ssh/authorized_keys/g' /etc/ssh/sshd_config && \
     echo "AuthorizedKeysFile /var/ssh/%u/authorized_keys" >> /etc/ssh/sshd_config && \
#     sed -ri 's/AuthorizedKeysFile/#AuthorizedKeysFile/g' /etc/ssh/sshd_config && \
#     echo "AuthorizedKeysFile /etc/ssh/%u/authorized_keys" >> /etc/ssh/sshd_config && \
     echo "UseDNS no" >> /etc/ssh/sshd_config && \
     useradd -m docker && mkdir /var/ssh/docker/ && \
     chown -R docker:docker /var/ssh/docker/ && \
     chmod 700 /var/ssh/docker/ && \
#     chmod 600 /var/ssh/docker/authorized_keys && \
     chmod 744 /root/start-sshd.sh && \
     ln -sf /usr/share/zoneinfo/America/New_York /etc/localtime && \
     apt-get clean)

# Virtue Custimization
# Uncomment to add repo based packages.
#__INSTALL

# Uncomment to add files from tarball
# Files should be provided as app.tar.gz containing the full path of the file(s).
#__TARGZ

# Virtue post install customization
# uncomment to blah
#__POSTINSTALL

#smaller size
RUN (apt-get clean)

EXPOSE 22
ENTRYPOINT ["/root/start-sshd.sh"]
