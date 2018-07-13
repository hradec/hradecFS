from base/archlinux:latest

MAINTAINER hradec <hradec@hradec.com>

# install needed packages
RUN     echo -e '\n\n[archlinuxfr]\nSigLevel = Never\nServer = http://repo.archlinux.fr/$arch\n\n' >> /etc/pacman.conf

RUN pacman -Syyuu --noconfirm

RUN pacman -S base-devel --noconfirm
RUN pacman -S nano --noconfirm
RUN pacman -S fuse3 sshfs gdb nemiver boost dbus python2 --noconfirm
RUN pacman -S virtualgl --noconfirm

RUN echo -e "#!/bin/bash\ndbus-daemon  --config-file=/usr/share/defaults/at-spi2/accessibility.conf --print-address &" > /run.sh ; chmod a+x /run.sh

RUN echo "root:t" | chpasswd


RUN ["/run.sh"]
