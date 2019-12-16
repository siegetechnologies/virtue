#!/bin/sh

for f in /lib/modules/`uname -r`/extra/core/*/*
  do
  /usr/local/bin/elf-mangler ${f}
done

modprobe mod_log uuid_in=$(uuidgen)
modprobe mod_listener
