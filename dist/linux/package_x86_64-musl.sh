#!/bin/bash

TOPDIR=../../bin/mudband/linux
VERSION=0.0.9

(cd ${TOPDIR} && make -f Makefile.musl clean && make -f Makefile.musl)

mkdir -p ../releases/${VERSION}/linux/

cp ${TOPDIR}/mudband ../releases/${VERSION}/linux/
strip ../releases/${VERSION}/linux/mudband
(cd ../releases/${VERSION}/linux/ &&
     tar -czvf mudband-${VERSION}-linux-x86_64-musl.tar.gz mudband &&
     rm -f mudband)
