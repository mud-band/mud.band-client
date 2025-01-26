#!/bin/sh

git clean -x -d -f .

CC=${HOME}/Sources/3rocks.net/mud.band-client/build/bin/musl-gcc CPP=TODO \
    ./Configure --prefix=${HOME}/Sources/3rocks.net/mud.band-client/build \
    linux-x86_64 no-tests no-shared
make && make install_sw
