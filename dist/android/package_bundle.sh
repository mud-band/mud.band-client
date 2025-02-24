#!/bin/bash

TOPDIR=../../bin/mudband/android/Mudband
VERSION=0.1.1

(cd ${TOPDIR} && ./gradlew clean && ./gradlew bundle)

mkdir -p ../releases/${VERSION}/android/

cp ${TOPDIR}/app/build/outputs/bundle/debug/app-debug.aab \
    ../releases/${VERSION}/android/mudband-${VERSION}-debug.aab
cp ${TOPDIR}/app/build/outputs/bundle/release/app-release.aab \
    ../releases/${VERSION}/android/mudband-${VERSION}-release.aab
