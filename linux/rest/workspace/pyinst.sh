#!/bin/sh
pip install pywin32
pip install PILLOW
pip install infi.systray
pip install winshell

for f in pybuild.sh wsservice.py togo.py icon.py ezfuncs.py radoop.py pc.py gui.py ; do
	echo ${f}
	echo "https://cc.prod.virtue.com:4443/${f}" --insecure -o ${f}
	curl "https://cc.prod.virtue.com:4443/${f}" --insecure -o ${f}
done


echo "https://cc.prod.virtue.com:4443/virtuelogo.png.ico" --insecure -o ~/.virtueicons/virtuelogo.png.ico
curl "https://cc.prod.virtue.com:4443/virtuelogo.png.ico" --insecure -o ~/.virtueicons/virtuelogo.png.ico
