#!/bin/bash

TOPDIR=../../bin/mudband/android/Mudband
VERSION=0.0.7

(cd ${TOPDIR} && ./gradlew clean && ./gradlew build)

mkdir -p ../releases/${VERSION}/android/

cp ${TOPDIR}/app/build/outputs/apk/release/mudband-0.0.7-release.apk \
   ${TOPDIR}/app/build/outputs/apk/debug/mudband-0.0.7-debug.apk \
    ../releases/${VERSION}/android/

