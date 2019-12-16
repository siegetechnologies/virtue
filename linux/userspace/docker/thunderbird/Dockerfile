FROM ubuntu:xenial
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update -y
RUN apt-get install -y thunderbird


RUN passwd -l root

RUN adduser virtue --shell /bin/rbash
RUN echo "virtue:123456" | chpasswd
USER virtue:virtue
ENV USER virtue
