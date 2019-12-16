(A) indicates acl command, otherwise, run on both machines
otherwise, run on both

# Installing from scratch (quick guide)
## Prereqs
apt-get install ruby ruby-dev make gcc
gem install fluentd fluent-plugin-secure-forward
(A) gem install fluent-plugin-s3
copy fluent-acl and fluent-node to /etc/fluent in the respective instances
edit fluent-node/fluent.conf to change secure-forward host to whatever the ACL address is

## secure forward
(A) secure-forward-ca-generate /etc/fluent/ <passphrase>
(A) scp /etc/fluent/ca_cert.pem <user@node>:/etc/fluent/ca_cert.pem
See fluent.conf files in fluent-acl and fluent-node, fill in the passphrase you used

## run on startup
useradd fluentd
copy https://gist.github.com/higebu/2904183 to /etc/init.d/fluentd and /etc/rc-1.d/fluentd
see https://github.com/fluent/fluentd/issues/1757 if you have issues running /etc/init.d/fluentd start
reboot, verify that fluentd is running

# If i want to change ___ do ___
ACL address or hostname
	change secure_forward host in fluent-node/fluent.conf
s3 bucket destination
	change s3 block in fluent-act/fluent.conf
acl certificate or passphrase
	re-run secure-forward-ca-generate on ACL, copy new ca_cert.pem to host, adjust fluent_*/fluent.conf with new passpharse (if applicable)


# Further reading
https://docs.fluentd.org/v0.12/articles/in_secure_forward
https://docs.fluentd.org/v0.12/articles/out_secure_forward


