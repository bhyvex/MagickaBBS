#!/usr/bin/env bash

printstuff() {
  YELLOW='\033[0;33m'
  NC='\033[0m'
  echo -e "${YELLOW}$1${NC}"
}

USERNAME=`whoami`
PWD=`pwd`

if [ ! -e ./ansis ]; then
    cp -r dist/ansis .
fi

if [ ! -e ./config ]; then
    cp -r dist/config .
fi

if [ ! -e ./menus ]; then
    cp -r dist/menus .
fi

if [ ! -e ./scripts ]; then
    cp -r dist/scripts .
fi

if [ ! -e ./magicka.strings ]; then
    ln -s dist/magicka.strings magicka.strings
fi

if [ ! -e ./www ]; then
    ln -s dist/www www
fi

if [ ! -e ./logs ]; then
    mkdir logs
fi

if [ ! -e ./msgs ]; then
    mkdir msgs
fi

if [ ! -e ./files ]; then
    mkdir -p files/misc
fi

printstuff "Please enter your real first and last name:"

read firstname lastname

printstuff "Please enter your user handle:"

read -e handle

printstuff "Please enter your location:"

read -e location

printstuff "Please enter the name of your BBS:"

read -e bbsname 

PLATFORM=`uname`

if [[ "$PLATFORM" == 'FreeBSD' ]] || [[ "$PLATFORM" == 'Darwin' ]]; then
    SED = gsed
else
    SED = sed
fi

$SED -i "s@/home/andrew/MagickaBBS@${PWD}@g" config/bbs.ini
$SED -i "s/BBS Name = Magicka BBS/BBS Name = ${bbsname}/g" config/bbs.ini
$SED -i "s/Sysop Name = sysop/Sysop Name = ${handle}/g" config/bbs.ini
$SED -i "s@/home/andrew/MagickaBBS@${PWD}@g" config/localmail.ini
$SED -i "s@/home/andrew/MagickaBBS@${PWD}@g" config/filesgen.ini
$SED -i "s@/home/andrew/MagickaBBS@${PWD}@g" config/illusionnet.ini
$SED -i "s@/home/andrew/MagickaBBS@${PWD}@g" utils/magiedit/magiedit.sh
$SED -i "s@/home/andrew/MagickaBBS@${PWD}@g" scripts/login_stanza.lua
$SED -i "s/MagiChat Server = localhost/; MagiChat Server = localhost/g" config/bbs.ini
$SED -i "s/Default Tagline = Brought to you by Another Magicka BBS!/Default Tagline = ${bbsname}/g" config/bbs.ini
$SED -i "s/echomail.out/mail.out/g" config/bbs.ini
$SED -i "s/netmail.out/mail.out/g" config/bbs.ini
