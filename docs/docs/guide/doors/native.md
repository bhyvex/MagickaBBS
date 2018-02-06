# Native Doors

Native doors for Linux/UN\*X are easy to set up. Magicka can run doors in two modes, *STDIO* mode and *Socket Inheritance* Mode. STDIO mode is the best as it will work on SSH as well as telnet.

## Script Files

To run a door, you should generally use a script file, as the BBS passes 2 options in specific order first, the node, and second in case of socket inheritance the socket number.

A typical script for running a door might look like this:

    #!/bin/sh

    NODE=$1
    SOCKET=$2

    cd /path/to/door
    ./somedoor -D /path/to/BBS/node${NODE}/door.sys

In addition to native doors, you can also run native programs as doors. You need to be careful with this though, as an incorrectly set up program could allow the user access to your file system.

The following script will telnet the user to exotica BBS

    #!/bin/sh

    telnet -E exoticabbs.com 2023

The -E switch causes telnet to have no "Escape" key, ie, the user cant break out of the connection and connect to arbitary systems.

## BBS Configuration

To actually run the door from the BBS, you will need to create an entry in your doors.ini, and add a menu item for that entry in the menu you want to launch it from. See doors.ini under INI files, and Menu Editing for more details.