#!/bin/bash
docker run --rm -v x11-xpra:/tmp/.X11-unix -e DISPLAY=:2 --entrypoint=/usr/bin/thunderbird thunderbird
