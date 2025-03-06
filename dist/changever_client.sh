#!/bin/sh

#
# All strings are the regular expressions.
#
PREV=0\\.1\\.2
NEXT=0\\.1\\.3
PREVC=100010200
NEXTC=100010300

changeit() {
	DST=$1
	if [ ! -f "$1" ]; then
		echo "[ERROR] No file exists: $1"
		exit 1
	fi
	sed -e "s/$PREV/$NEXT/g" "$DST" > .tmpfile
	cp .tmpfile "$DST"
}

changever() {
	DST=$1
	if [ ! -f "$1" ]; then
		echo "[ERROR] No file exists: $1"
		exit 1
	fi
	sed -e "s/$PREVC/$NEXTC/g" "$DST" > .tmpfile
	cp .tmpfile "$DST"
}

changeit ../bin/mudband/android/Mudband/app/build.gradle.kts
changeit ../bin/mudband/android/Mudband/app/src/main/java/band/mud/android/ui/UiMudbandApp.kt
changeit ../bin/mudband/linux/mudband.c
changeit ../bin/mudband/win32/mudband.c
changeit ../bin/mudband/win32/NMakefile
changeit ../bin/mudband/win32/NMakefile.x64
changeit ../bin/mudband_service/linux/debian/changelog
changeit ../bin/mudband_service/windows/NMakefile.x64
changeit ../bin/mudband_ui/linux/package.json
changeit ../bin/mudband_ui/linux/src-tauri/Cargo.toml
changeit ../bin/mudband_ui/linux/src-tauri/tauri.conf.json
changeit ../bin/mudband_ui/windows/NMakefile
changeit ../bin/mudband_ui/windows/package.json
changeit ../bin/mudband_ui/windows/src-tauri/Cargo.toml
changeit ../bin/mudband_ui/windows/src-tauri/tauri.conf.json

changeit android/package_apk.sh
changeit android/package_bundle.sh
changeit linux/package_service_x86_64-deb.sh
changeit linux/package_ui_x86_64.sh
changeit linux/package_x86_64-musl.sh
changeit windows/package_single_x64.bat
changeit windows/package_single_x86.bat
changeit windows/package_ui_x64.bat
changeit windows/wix/package.wxs

changever ../bin/mudband/android/Mudband/app/build.gradle.kts
