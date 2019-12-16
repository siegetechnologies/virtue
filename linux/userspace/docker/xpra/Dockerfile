FROM ubuntu:xenial
ENV DEBIAN_FRONTEND noninteractive

EXPOSE 6000

RUN apt-get update -y
RUN apt-get install xpra -y


RUN adduser virtue --shell /bin/rbash
RUN echo "virtue:123456" | chpasswd
USER virtue:virtue
ENV USER virtue

RUN mkdir /home/virtue/.xpra
RUN mkdir /home/virtue/x11socket

ENV DISPLAY :2

ENTRYPOINT xpra start ${DISPLAY} --daemon=no --speaker=no --pulseaudio=no --socket-dir=/home/virtue/x11socket --xvfb="Xorg -listen tcp -config /etc/xpra/xorg.conf +extension GLX +extension RANDR +extension RENDER -logfile /home/virtue/.xpra/${DISPLAY}.log"
