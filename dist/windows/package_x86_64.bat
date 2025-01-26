set VERSION=0.0.6

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd ..
mkdir releases\%VERSION%\windows\x86_64
cd ..\bin\mudband
nmake -f NMakefile.x64 clean
nmake -f NMakefile.x64 BUILD=RELEASE
signtool sign /fd sha256 /n Mudfish /t http://timestamp.comodoca.com/authenticode mudband.exe
copy mudband.exe ..\..\dist\releases\%VERSION%\windows\x86_64\
cd ..\..\
copy contrib\prebuilt\wintun\bin\amd64\wintun.dll dist\releases\%VERSION%\windows\x86_64\
cd dist\releases\%VERSION%\windows\x86_64\
7z a mudband-%VERSION%-windows-x86_64.zip mudband.exe wintun.dll
cd ..\..\..\..\..\bin\mudband
copy mudband.pdb ..\..\dist\releases\%VERSION%\windows\x86_64
