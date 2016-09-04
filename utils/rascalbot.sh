#!/bin/bash

# BEGIN CONFIGURATION--------------------------------------------------
MGPOST=/home/pi/MagickaBBS/utils/mgpost/mgpost
NODE=21:1/140
AREA=/home/pi/MagickaBBS/msgs/fsxnet/fsx_bot
ORIGINTXT="Underland - telnet://andrew.homeunix.org:2023"
FROM=rascal-bot
SEMAPHORE=/home/pi/MagickaBBS/echomail.out
# END CONFIGURATION----------------------------------------------------

SUBJECT=`head -1 $1`
TMP=/tmp/$RANDOM-rascal

mkdir -p $TMP

tail -n +2 $1 | cat >  $TMP/rascal.txt

echo "" >> $TMP/rascal.txt
echo "--- rascal-bot (v0.1)" >> $TMP/rascal.txt
echo " * Origin: $ORIGINTXT ($NODE)" >> $TMP/rascal.txt

$MGPOST e $TMP/rascal.txt "$AREA" "$FROM" "$SUBJECT" $NODE
touch $SEMAPHORE

rm -rf $TMP
