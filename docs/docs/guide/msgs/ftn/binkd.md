# Installing and configuring BinkD

## Installation

Magicka does not include binkd, you have to get it and install it yourself. You could use a package, but packages aren't available on all platforms, so this is how to install it from source.

First clone the repo:

    git clone https://github.com/pgul/binkd

Next configure binkd:

    cd binkd
    cp -r mkfls/unix/* .
    ./configure

Then build

    make

And install into /usr/local/sbin

    sudo make install

## Configuration

First, make the following subdirectories in the MagickaBBS directory:

    mkdir -p ftn/out
    mkdir -p ftn/in_sec
    mkdir -p ftn/in
    mkdir -p ftn/in_temp


Next you need to create a configuration file which will be used when running the program.

I place mine in `MagickaBBS/ftn/binkd.conf`

The following sample configuration is an example of a single network setup, feel free to copy it and substitute your details.

### Sample Configuration

    # Number @ end is the root zone
    domain happynet /path/to/MagickaBBS/ftn/out 637


    # Our HappyNet address
    address 637:1/999@happynet

    sysname "Super BBS"
    location "Somewhere"
    sysop "Your Name"

    nodeinfo 115200,TCP,BINKP
    try 10
    hold 600
    send-if-pwd

    log /path/to/MagickaBBS/logs/binkd.log
    loglevel 4
    conlog 4
    percents
    printq
    backresolv

    inbound /path/to/MagickaBBS/ftn/in_sec
    inbound-nonsecure /path/to/MagickaBBS/ftn/in
    temp-inbound /path/to/MagickaBBS/ftn/in_temp

    minfree 2048
    minfree-nonsecure 2048

    kill-dup-partial-files
    kill-old-partial-files 86400

    prescan

    # Happynet HUB
    node 637:1/100@happynet -md hnet.magickabbs.com:24554 SECRET c

    # our listening port (default=24554)
    iport 24554

    pid-file /path/to/MagickaBBS/ftn/binkd.pid

    # touch a watch file when files are received to kick of toss
    exec "/path/to/MagickaBBS/ftn/magitoss.sh"  *.[mwtfs][oehrau][0-9a-zA-Z] *.pkt
    exec "/path/to/MagickaBBS/ftn/ticproc.sh" *.tic *.TIC

    # nuke old .bsy/.csy files after 24 hours
    kill-old-bsy 43200

NOTE: `magitoss.sh` and `ticproc.sh` will be detailed in the magimail setup guide and the tic file processing setup guide.

## Configuring for multiple networks

If you want to be a member of multiple networks, you need additional domain, address and node lines. See the following example for HappyNet and FSXNet

### Sample Configuration

    # Number @ end is the root zone
    domain happynet /path/to/MagickaBBS/ftn/out 637
    domain fsxnet /path/to/MagickaBBS/ftn/fsxnet 637

    # Our HappyNet address
    address 637:1/999@happynet
    # Our FSXNet Address
    address 21:1/999@fsxnet

    sysname "Super BBS"
    location "Somewhere"
    sysop "Your Name"

    nodeinfo 115200,TCP,BINKP
    try 10
    hold 600
    send-if-pwd

    log /path/to/MagickaBBS/logs/binkd.log
    loglevel 4
    conlog 4
    percents
    printq
    backresolv

    inbound /path/to/MagickaBBS/ftn/in_sec
    inbound-nonsecure /path/to/MagickaBBS/ftn/in
    temp-inbound /path/to/MagickaBBS/ftn/in_temp

    minfree 2048
    minfree-nonsecure 2048

    kill-dup-partial-files
    kill-old-partial-files 86400

    prescan

    # Happynet HUB
    node 637:1/100@happynet -md hnet.magickabbs.com:24554 SECRET c

    # FSXNet HUB
    node 21:1/100@fsxnet -md ipv4.agency.bbs.geek.nz:24556 LETMEIN c

    # our listening port (default=24554)
    iport 24554

    pid-file /path/to/MagickaBBS/ftn/binkd.pid

    # touch a watch file when files are received to kick of toss
    exec "/path/to/MagickaBBS/ftn/magitoss.sh"  *.[mwtfs][oehrau][0-9a-zA-Z] *.pkt
    exec "/path/to/MagickaBBS/ftn/ticproc.sh" *.tic *.TIC

    # nuke old .bsy/.csy files after 24 hours
    kill-old-bsy 43200

NOTE: `magitoss.sh` and `ticproc.sh` will be detailed in the magimail setup guide and the tic file processing setup guide.

## Running Binkd

To run binkd, simply execute:

    /usr/local/sbin/binkd /path/to/MagickaBBS/ftn/binkd.conf

You could run this in screen, or setup a service for your OS.
