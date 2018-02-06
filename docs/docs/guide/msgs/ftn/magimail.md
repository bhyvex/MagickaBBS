# Magimail configuration

TODO.

## Example magimail.prefs file

    SYSOP "Your Name"
    BBSNAME "Some BBS"

    BROADCAST 192.168.1.255 2025

    LOGFILE "/path/to/MagickaBBS/logs/magimail.log"
    LOGLEVEL 3

    DUPEFILE "/path/to/MagickaBBS/ftn/dupes.log" 200
    DUPEMODE BAD

    LOOPMODE LOG+BAD

    MAXPKTSIZE 50
    MAXBUNDLESIZE 100

    DEFAULTZONE 637

    INBOUND "/path/to/MagickaBBS/ftn/in_sec"
    OUTBOUND "/path/to/MagickaBBS/ftn/out"
    TEMPDIR "/path/to/MagickaBBS/ftn/cm_temp"
    CREATEPKTDIR "/path/to/MagickaBBS/ftn/cm_temp"
    PACKETDIR "/path/to/MagickaBBS/ftn/cm_packets"
    STATSFILE "/path/to/MagickaBBS/ftn/magimail.stats"
    FORCEINTL
    ANSWERRECEIPT
    ANSWERAUDIT
    CHECKSEENBY
    PATH3D
    IMPORTAREAFIX
    IMPORTSEENBY
    AREAFIXREMOVE
    WEEKDAYNAMING
    ADDTID
    ALLOWRESCAN
    INCLUDEFORWARD
    ALLOWKILLSENT

    GROUPNAME A "happynet"

    PACKER "ZIP" "zip -j %a %f" "unzip -j %a" "PK"

    AKA 637:1/999
    DOMAIN happynet

    NODE 637:1/100 "ZIP" "" AUTOADD PACKNETMAIL
    DEFAULTGROUP A

    ROUTE "637:*/*.*" "637:1/100.0" 637:1/999

    JAM_HIGHWATER
    JAM_LINK
    JAM_QUICKLINK
    JAM_MAXOPEN 5
    NETMAIL "NETMAIL" 637:1/999 JAM "/path/to/MagickaBBS/msgs/hpy_netmail"

    AREA "BAD" 637:1/999 JAM "/path/to/MagickaBBS/msgs/bad"

    AREA "DEFAULT_A" 637:1/999 JAM "/path/to/MagickaBBS/msgs/%l"

    AREA "HPY_GEN" 637:1/999.0 JAM "/path/to/MagickaBBS/msgs/hpy_gen"
    EXPORT %637:1/100.0
    GROUP A
