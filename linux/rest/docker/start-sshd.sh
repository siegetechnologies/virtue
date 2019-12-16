#!/bin/sh

#mkdir -p /var/storagekeys/
#chown -R docker:docker /var/storagekeys/

#todo if -z these
#echo "${VIRTUE_USER_AUTH_KEY}" > /var/storagekeys/user
#echo "${VIRTUE_PERSISTENT_AUTH_KEY}" > /var/storagekeys/persistent


USER_HOME=/home/docker

#recursively own my home
chown -R docker:docker $USER_HOME/


mkdir -p /var/ssh/docker/
chmod 700 /var/ssh/docker/
chown docker:docker /var/ssh/docker/
echo ${VIRTUE_AUTH_SSH}
echo ${VIRTUE_AUTH_SSH} >> /var/ssh/docker/authorized_keys
chown docker:docker /var/ssh/docker/authorized_keys
chmod 600 /var/ssh/docker/authorized_keys






#mkdir -p /root/.ssh_kubestuff
#rm -rf /etc/ssh/docker
#cp -r /root/.ssh_kubestuff /etc/ssh/docker
#chmod 700 /etc/ssh/docker
#chown docker:docker /etc/ssh/docker
#chmod 644 /etc/ssh/docker/authorized_keys
#chown docker:docker /etc/ssh/docker/authorized_keys


#mkdir -p /root/.ssh_kubestuff
#rm -rf $USER_HOME/.ssh
#cp -r /root/.ssh_kubestuff $USER_HOME/.ssh
#chmod 700 $USER_HOME/.ssh
#chown docker:docker $USER_HOME/.ssh

#if [ -f "/app.tar.gz" ] ; then
#  tar -zxpvf /app.tar.gz -C /
#fi

# Start sshd
/usr/sbin/sshd -D
