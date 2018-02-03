# DOS doors With DosBox

This guide is aimed at Raspbian Linux (Stretch) on Raspberry PI, but should work on any Linux/UNIX like system DosBox supports.

## Step 1. Install Pre-Requisites

To compile DosBox, you will need SDL 1.2 and SDL-net in addition to the usual development toolchain. You will also need Subversion to get dosbox and patch to patch it.

    sudo apt-get install build-essential libsdl1.2-dev libsdl-net1.2-dev subversion automake dos2unix

## Step 2. Fetch DosBox

The most recent DosBox release has some issues with DOS doors, and the most recent Subversion code also has issues, so you will need a specific revision from the subversion repository. In my tests I have found r3933 to work well. Other revisions may work, but have not been tested.

    svn checkout -r3933 svn://svn.code.sf.net/p/dosbox/code-0/dosbox/trunk dosbox-code-0

## Step 3. Patch DosBox 

Now we have the DosBox code, you will probably want to patch it to fix a specific bug that happens with some doors which will appear as though when you press enter, it receives the enter command twice.

    wget https://gist.githubusercontent.com/apamment/bb438d9be6dc8e67c36239fd64047ece/raw/3e942c68d7a970f3404bffc2408165d810c4bef7/dosbox-nullmodem.diff
    
    cd dosbox-code-0
    
    patch -p1 < ../dosbox-nullmodem.diff


## Step 4. Compile DosBox

Compiling DosBox takes a long time on the Raspberry PI and will appear to have frozen a few times - particularly when compiling the render-scalers file - don't worry, just leave it it will eventually move on.

    ./autogen.sh
    ./configure
    make
    sudo make install

These commands will compile and install DosBox into /usr/local/bin.

## Step 5. Create Configuration File

Next you will need to create a config file which enables the serial port for socket inheritance, and also mounts drives so that DosBox can access both your doors and your BBS drop files.

An example setup is here: [[https://gist.github.com/apamment/98e42db83c452105b3e21a8bc062c5c3|dosbox.conf]]

In this example, drive C: is the location where my doors are stored, dropfiles will be copied to the door directory.

## Step 6. Create Shell Script to Invoke DosBox

The shell script you need will vary from system to system, but basically, you want to:

  *  ensure no one else is using the door before you start it
  *  Copy the dropfile to the door directory
  *  make DosBox not use an X11 window, 
  *  ensure that the dropfiles have DOS line endings
  *  launch DosBox with the command to start the batch file in the next section.

Here is an example script I use for Freshwater Fishing:

    #!/bin/bash
    export SDL_VIDEODRIVER="dummy"
    NODE=$1
    SOCKET=$2
    
    if [ ! -e /home/pi/MagickaBBS/doors/ffs.inuse ]; then
       touch /home/pi/MagickaBBS/doors/ffs.inuse
       cp /home/pi/MagickaBBS/node${NODE}/door.sys /home/pi/MagickaBBS/doors/ffs/
       /usr/local/bin/dosbox -socket $SOCKET -c "C:\ffs\ffs.bat ${NODE}" -conf /home/pi/MagickaBBS/doors/dosbox.conf
       rm /home/pi/MagickaBBS/doors/ffs.inuse
    fi


This will just dump the user back to the BBS if the door is in use. You could get fancy and use the 'inuse' door I created which will display an in-use message and then quit ([[https://github.com/apamment/inuse|INUSE Door]]), but that is outside the scope of this document.

You will also need to make the bash script executable:

    chmod +x ffs.sh

## Step 7. Create Batch File to Start Door

Please note, you will need to setup your doors using DosBox in the normal fashion, either you set them up on a machine where you have a monitor attached and transfer the files over, or you run DosBox directly from your PI with either remote X11 or a local monitor.

You will also most likely need a Fossil driver, [[http://www.pcmicro.com/bnu/|BNU]] works well. (Get the stable release 1.70).

Different doors will need to be called in different ways, but the important thing to remember is that you end the batch file with "exit" else the door will stay stuck in DosBox.

Here is an example I use for freshwater fishing:

    @echo off
    C:
    cd\bnu
    bnu /L0=11520
    cd\ffs
    fishing 2 C:\ffs\  /F
    exit

This loads BNU, then launches the door with the drop file.

## Step 8. Setup BBS

Finally, you will want to setup your BBS to call the script you made in Step 6. You also need to pass the node number and socket handle.

In doors.ini, something like

    [Freshwater Fishing]
    command = /home/pi/MagickaBBS/doors/ffs.sh
    stdio = false
    codepage = CP437

Then, add a command to your doors menu:

    HOTKEY 1
    COMMAND RUNDOOR
    DATA Freshwater Fishing

