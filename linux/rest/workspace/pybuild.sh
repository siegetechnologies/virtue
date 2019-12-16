#!/bin/sh
./pyinst.sh

pip install pyinstaller
pyinstaller --onefile --noconsole -i ~/.virtueicons/virtuelogo.png.ico -F ./wsservice.py
md5sum ./dist/wsservice.exe
