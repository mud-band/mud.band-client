#!/bin/sh

#
# All strings are the regular expressions.
#
PREV=0\\.0\\.6
NEXT=0\\.0\\.7
PREVC=0x000006
NEXTC=0x000007

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
changeit linux/dist_x86_64.sh
changeit linux/package_x86_64.sh
changeit tags_client.sh
changeit windows/dist.sh
changeit windows/package_x86.bat
changeit windows/package_x86_64.bat

changever ../bin/mudband/android/Mudband/app/build.gradle.kts
