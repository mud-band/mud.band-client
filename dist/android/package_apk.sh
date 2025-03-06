#!/bin/bash

TOPDIR=../../bin/mudband/android/Mudband
VERSION=0.1.3

(cd ${TOPDIR} && ./gradlew clean && ./gradlew build)

mkdir -p ../releases/${VERSION}/android/

cp ${TOPDIR}/app/build/outputs/apk/release/mudband-${VERSION}-release.apk \
   ${TOPDIR}/app/build/outputs/apk/debug/mudband-${VERSION}-debug.apk \
    ../releases/${VERSION}/android/

