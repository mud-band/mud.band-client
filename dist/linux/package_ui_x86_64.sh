#!/bin/bash

TOPDIR=../../bin/mudband_ui/linux
BUNDLEDIR=${TOPDIR}/src-tauri/target/release/bundle
VERSION=0.1.2

(cd ${TOPDIR} && make build)

mkdir -p ../releases/${VERSION}/linux/

cp ${BUNDLEDIR}/deb/mudband-ui_${VERSION}_amd64.deb \
    ../releases/${VERSION}/linux/
cp ${BUNDLEDIR}/rpm/mudband-ui-${VERSION}-1.x86_64.rpm \
    ../releases/${VERSION}/linux/
cp ${BUNDLEDIR}/appimage/mudband-ui_${VERSION}_amd64.AppImage \
    ../releases/${VERSION}/linux/
