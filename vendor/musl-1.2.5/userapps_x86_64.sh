#!/bin/sh

git clean -x -d -f .

./configure \
    --prefix=${HOME}/Sources/3rocks.net/mud.band-client/build \
    --enable-wrapper --disable-shared
make && make install
