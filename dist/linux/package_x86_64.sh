#!/bin/bash

VERSION=0.0.7

(cd ../../bin/mudband && make -f Makefile.musl clean && make -f Makefile.musl)

mkdir -p ../releases/${VERSION}/linux/

cp ../../bin/mudband/mudband ../releases/${VERSION}/linux/
strip ../releases/${VERSION}/linux/mudband
(cd ../releases/${VERSION}linux/ &&
     tar -czvf mudband-${VERSION}-linux-x86_64.tar.gz mudband &&
     rm -f mudband)
