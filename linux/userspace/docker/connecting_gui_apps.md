#Connecting GUI apps with xpra

## Motivation
X11 is inherently insecure. Xorg socket in /tmp/.X11-unix/ is shared by all GUI
apps, programs with access to the socket can have full control over the entire
screen.
Read-only access still allows programs such as xinput to keylog, potentially 
sniffing passwords or important secrets.
Since it is naive to assume that applications will not be compromised, 
it is imperative that apps are sandboxed.
Unfortunately, the only way to do this is to give each app it's own X11 server.
The tool we use for this is xpra.

## Topology
Each app is isolated by running in its own docker container.
Each app has a dedicated xpra server, also running containerized alongside the
app's container
xpra creates an x11 socket in /tmp/.X11-unix
The app wants to have an X11 socket depending on the value of DISPLAY. For
DISPLAY=:2, that socket is /tmp/.X11-unix/X2
The two sockets are merged by binding both containers /tmp/.X11-unix 
directories to a shared volume created with 'docker volume create'.
In the test system, the volume is called x11-xpra (but the name isn't really
important)
xpra creates a socket that can be attached to. 
Using the --socket-dir flag, we can tell that socket where to be created.
We create the socket in a subdirectory of the user's home.
This directory is also volume-mounted to somewhere on the host system, in our
case: /home/ubuntu/sockets.
When the container starts up, a socket file should appear in this directory,
it can be attached to with 'xpra attach socket:<socketfile>'

## To run
Create shared volume created with 'docker volume create x11-xpra'
Build images with 'docker build ./thunderbird -t thunderbird' and 'docker build ./xpra -t xpra'
Create local directory /home/ubuntu/sockets
Run xpra/xpra.sh
Run thunderbird/run.sh
Run 'docker attach ~/sockets/<socketfile>'

## Misc
The DISPLAY is hardcoded as :2. This choice is arbitrary.
If multiple xpra containers are started, there will be conflicts due to the hardcoded display.
We recommend creating a new shared docker volume for each app-xpra pair,
although ideally, only 1 app runs on each instance.
Docker hostnames randomly generated. There may be benefits to setting this.
Pulseaudio is unsupported.
Speakers are turned off due to their high CPU requirement. It doesn't help
