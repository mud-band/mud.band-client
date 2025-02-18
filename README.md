# Mud.band client

This is a git repo for Mud.band client.  At this moment the following
platforms are supported:

* Android
* iOS
* Linux
* macOS
* Windows

## Build

### Android

If you're a user of android studio, open `bin/mudband/android/Mudband/`
directory and build.

If you're a user of gradle, you can build it with the following command:

```
cd ${TOPDIR}/bin/mudband/android/Mudband/
./gradlew build
```

### iOS

Open `bin/mudband/ios/Mud.band\ -\ Mesh\ VPN/Mud.band\ -\ Mesh\ VPN.xcodeproj/`
with XCode and build.

### Linux

#### Mudband

`mudband` binary is a core binary to communicate with peers. If you want to
use glibc:

```
cd ${TOPDIR}/bin/mudband/linux
make clean && make
```

If you want to use musl:

```
(cd ${TOPDIR}/vendor/musl-1.2.5/ && userapps_x86_64.sh)
(cd ${TOPDIR}/vendor/openssl-3.4.0/ && userapps_x86_64.sh)
cd ${TOPDIR}/bin/mudband/linux
make -f Makefile.musl
```

#### Mudband Service

`mudband_service` process is a control binary to start / stop `mudband`
process.  To this this binary,

```
cd ${TOPDIR}/bin/mudband_service/linux
make clean && make
```

#### Mudband UI

`mudband_ui` is a UI process based on Tauri.  To build the binary,

```
# for dev environment
cd ${TOPDIR}/bin/mudband_ui/linux
make init && make
# for binary build
cd ${TOPDIR}/bin/mudband_ui/linux
make init && make build
```

### macOS

Open `bin/mudband/macos/Mudband/Mud.band.xcodeproj/` with XCode
and build.

### Windows

In windows, there're three parts; `mudband.exe`, `mudband_service.exe`
and `mudband_ui.exe`

#### mudband.exe

Open the command prompt with Visual Studio dev environment.

##### x86

```
cd bin\mudband\win32
nmake -f NMakefile
```

##### x64

```
cd bin\mudband\win32
nmake -f NMakefile.x64
```

### mudband_service.exe

##### x64

```
cd bin\mudband_service\windows
nmake -f NMakefile.x64
```

### mudband_ui.exe

Mudband UI is based on Tauri.  So at least it requires Tauri dev environment
to build it properly.

##### x64

```
cd bin\mudband_ui\windows
nmake -f NMakefile init
nmake -f NMakefile build
```

## Packaging

### Android, Linux and Windows

For details, please check the following directory and the scripts at

* `${TOPDIR}/dist/android`
* `${TOPDIR}/dist/linux`
* `${TOPDIR}/dist/windows`

### iOS and macOS

Please use XCode to archive the package build.
