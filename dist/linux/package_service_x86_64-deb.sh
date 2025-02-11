#!/bin/bash

VERSION=0.0.9
TOPDIR=../../bin/mudband_service/linux

(cd ${TOPDIR} && make pkg_deb)

mkdir -p ../releases/${VERSION}/linux/

cp ${TOPDIR}/../mudband-service_${VERSION}-1_amd64.deb \
    ../releases/${VERSION}/linux/
