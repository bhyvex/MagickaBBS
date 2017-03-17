# MagickaBBS

[![Build Status](https://build.magickabbs.com/buildStatus/icon?job=MagickaBBS-Linux)](https://build.magickabbs.com/job/MagickaBBS-Linux)

Linux/FreeBSD bulletin board system (Should also work on NetBSD and OpenBSD, if it doesn't it's a bug)

As I lost the code to my initial BBS flea, I've decided to start over from scratch and this time I'm using git hub so I dont
lose it again!

Magicka is meant to be a modern (haha) BBS system, using modern technologies, like Sqlite3, IRC, long filenames (gasp!) etc
while still retaining the classic BBS feel. ANSI & Telnet, and good old ZModem.

If you want to install Magicka BBS, follow these steps.

1. Ensure you have git, c compiler, libsqlite3-dev, libreadline-dev, libssl-dev, libssh-dev and gnu make

   `sudo apt-get install build-essential libsqlite3-dev libreadline-dev git libssl-dev libssh-dev`

   should work on debian and debian derivatives.
2. Clone the repo `git clone https://github.com/MagickaBBS/MagickaBBS`
3. Build JamLib

  `cd MagickaBBS/jamlib`


  `make -f Makefile.linux` (Linux) `gmake -f Makefile.linux` (*BSD)

3. Build libzmodem

  `cd MagickaBBS/Xmodem`


  `make` (Linux) `gmake` (*BSD)

4. Build LUA

	`cd MagickaBBS/lua`

	`make linux` (Linux) `gmake freebsd` (FreeBSD)

5. Build the BBS (You may have to adjust the Makefile for your system)

  `make -f Makefile.linux` (Linux) `gmake -f Makefile.freebsd` (FreeBSD)

6. Make a directory for logs.

	`mkdir logs`

7. Copy the config-default directory to a config directory.

  `cp -r config_default config`

8. Edit the config files and update essential information, like system paths and BBS name etc
9. Copy the ansi-default directory to the one specified in your system path

  eg.

  `cp -r ansi_default ansis`

10. If you are going to run SSH, you will need to create keys. To do this

  `mkdir keys`

	`ssh-keygen -f keys/ssh_host_rsa_key -N '' -t rsa`
	
	`ssh-keygen -f keys/ssh_host_dsa_key -N '' -t dsa`

11. Run Magicka BBS on a port over 1024 (Below require root, and we're not ready for that).

  `./magicka config/bbs.ini`

12. Your BBS is now running on the port you specified in the config.ini, log in and create yourself an account! (By default there is only one security level, you can add more,
but you will need to use an SQLite Manager to modify users.sq3 and set security levels, as there is no user editor yet.

For information on how to configure your BBS, check the wiki https://github.com/MagickaBBS/MagickaBBS/wiki

# About the webserver

Magicka now includes a built in webserver based on libmicrohttpd. It is not built by default, if you'd like to build it you will
need a recent version of libmicrohttpd (raspbian doesn't have a recent enough version, and you will need to build it from source).
You will also need libb64-dev. Once you have these prerequisites, you can build magicka with `make -f Makefile.linux.WWW` or 
`make -f Makefile.freebsd.WWW` be sure to do this from a clean source tree.

The webserver will use templates in the www/ folder to create internal webpages on the fly, anything in www/static/ is served up as is.

