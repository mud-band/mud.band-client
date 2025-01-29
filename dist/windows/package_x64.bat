set VERSION=0.0.7

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd ..
mkdir releases\%VERSION%\windows\x64
cd ..\bin\mudband\win32
nmake -f NMakefile.x64 clean
nmake -f NMakefile.x64 BUILD=RELEASE
signtool sign /fd sha256 /n Mudfish /t http://timestamp.comodoca.com/authenticode mudband.exe
copy mudband.exe ..\..\..\dist\releases\%VERSION%\windows\x64\
copy mudband.pdb ..\..\..\dist\releases\%VERSION%\windows\x64\
cd ..\..\..\
copy contrib\prebuilt\wintun\bin\amd64\wintun.dll dist\releases\%VERSION%\windows\x64\
cd dist\releases\%VERSION%\windows\x64\
7z a mudband-%VERSION%-windows-x64.zip mudband.exe mudband.pdb wintun.dll
