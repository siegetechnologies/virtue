#!/bin/bash
MAVEN_DIR=/lib/modules/$(uname -r)/extra
ELF_MANGLER=../userspace/elf-mangler/elf-mangler
MANGLER_OPTS=-sbCM

entries=$(find ${MAVEN_DIR} -regex '.*\.ko')

for entry in ${entries}
do
	cmd="sudo ${ELF_MANGLER} ${entry} ${MANGLER_OPTS} ${entry}"
	echo ${cmd}
	${cmd}
done

