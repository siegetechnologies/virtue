#!/bin/sh
make clean
for i in `seq 1 100`; do
#	sleep 1
	rm -f *.o.mod
#	make clean
	if ! make -j16 ; then
		echo "Testing failed at ${i}"
		exit 1
	fi
done
