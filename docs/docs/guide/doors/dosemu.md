# Dos Doors with DOSEMU

DOSEMU only runs on Linux/x86, if you're running on another platform, try the DOSBOX tutorial.

In this example, I will be configuring for MagickaBBS and Tiny's Hangman game.

## Install DOSEMU

Depending on your distribution, this command will be different. On ubuntu:

    sudo apt-get install dosemu

Should do the trick.

## Configure DOSEMU

First, run dosemu once to create /home/someuser/.dosemu/ paths then modify the supplied autoexec.bat.


This is my autoexec.bat (in /home/someuser/.dosemu/drive_c/)

    @echo off
    break=off
    path z:\bin;z:\gnu;z:\dosemu
    set HELPPATH=z:\help
    set TEMP=c:\tmp
    prompt $P$G
    lredir del d: > nul
    lredir d: linux\fs/home/someuser/MagickaBBS/doors
    unix -e



## Configure a Door

Firstly you want a dosemu.conf for calling the door, I use the same one for calling all my dos doors.

This is my dosemu.conf (in /home/someuser/MagickaBBS/doors/)

    $_cpu = "80486"
    $_cpu_emu = "vm86"
    $_external_char_set = "utf8"
    $_internal_char_set = "cp437"
    $_term_updfreq = (8)
    $_layout = "us"
    $_rawkeyboard = (0)
    $_com1 = "virtual"

You will need both [THANG23B.ZIP](http://tinysbbs.com/files/tsoft/THANG23B.ZIP) and [BNU170.ZIP](http://www.pcmicro.com/bnu/bnu170.zip)

First unzip BNU170.ZIP in /home/someuser/MagickaBBS/doors/bnu

Then unzip THANG23B.ZIP in /home/someuser/MagickaBBS/doors/thang

copy the executables from /home/someuser/MagickaBBS/doors/thang/dos to /home/someuser/MagickaBBS/doors/thang

Next Configure Tiny's Hangman by editing THANG.CFG. Note: I use dorinfo1.def but you could just as easily use door.sys.

Now create a batch file that we will use to launch tiny's hangman from DOS.

    @echo off
    D:
    CD \BNU
    BNU /L0=11520
    CD \THANG
    THANG /N%1 /DD:\THANG\NODE%1
    exitemu

create the following sub directories in THANG

    mkdir node1
    mkdir node2
    mkdir node3
    mkdir node4
    
until you have a directory for each node.

Finally, the script for running tinys hangman, thang.sh

    #!/bin/bash
    trap '' 2
    NODE=$1
    
    cp /home/someuser/MagickaBBS/node${NODE}/dorinfo1.def /home/someuser/MagickaBBS/doors/thang/node${NODE}/
    
    /usr/bin/dosemu -quiet -f /home/someuser/MagickaBBS/doors/dosemu.conf -I "dosbanner 0" -E "D:\THANG\THANG.BAT ${NODE}" 2>/dev/null
    
    trap 2


You can then edit your doors.ini to include:

    [Tinys Hangman]
    command = /home/someuser/MagickaBBS/doors/thang.sh
    stdio = true

and finally edit doors.mnu

    HOTKEY 1
    COMMAND RUNDOOR
    DATA Tinys Hangman

Then restart Magicka and you should be able to run Tinys Hangman by pressing 1 on the doors menu.