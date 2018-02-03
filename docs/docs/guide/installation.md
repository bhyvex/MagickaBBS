# Installation


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