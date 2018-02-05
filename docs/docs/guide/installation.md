# Installation

## Platform Specific Notes:

* FreeBSD requires libiconv to be installed from ports in addition to other dependencies.

* macOS makefiles are intended to be built with dependencies provided by macports, homebrew support will require significant changes.

* NetBSD requires libiconv to be installed from pkgsrc in addition to other dependencies. 

* OpenBSD as of 6.2 does not have libmicrohttpd in ports, but current does. Requires libiconv.

* OpenIndiana does not have libssh in packages, so it needs to be built separately. Requires libiconv.

* DragonFlyBSD also requires libiconv.

## Install Prerequisites

Ensure you have git, c compiler, libsqlite3-dev, libreadline-dev, libssl-dev, libssh-dev libncurses5-dev, libmicrohttpd-dev, bash and gnu make

    sudo apt-get install build-essential libsqlite3-dev libreadline-dev git libssl-dev libssh-dev libncurses5-dev libmicrohttpd-dev

should work on debian and debian derivatives.

## Install Magicka

Clone the repo 

    git clone https://github.com/MagickaBBS/MagickaBBS

Build the BBS

    make www

Run setup.sh and answer the questions to install the initial configurations

    ./setup.sh


If you are going to run SSH, you will need to create keys. To do this

    mkdir keys

    ssh-keygen -f keys/ssh_host_rsa_key -N '' -t rsa

    ssh-keygen -f keys/ssh_host_dsa_key -N '' -t dsa

## Run Magicka

Run BBS 

    ./magicka config/bbs.ini

The BBS will run by default on telnet only and on port 2023. Magicka must be run on a port over 1024, the port can be configured in bbs.ini. Log on to your new bbs and create your sysop account!