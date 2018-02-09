#!/usr/bin/env sh

wget https://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.59.tar.gz
tar xzf libmicrohttpd-0.9.59.tar.gz
cd libmicrohttpd-0.9.59
./configure
make
make install
