#!/bin/sh

git clean -x -d -f .

CC=${HOME}/Sources/3rocks.net/mudband/build/bin/musl-gcc CPP=TODO \
    ./Configure --prefix=${HOME}/Sources/3rocks.net/mudband/build \
    linux-x86_64 no-tests no-shared
make && make install_sw
