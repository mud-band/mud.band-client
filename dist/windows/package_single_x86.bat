set VERSION=0.1.2
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
cd ..
mkdir releases\%VERSION%\windows\x86
cd ..\bin\mudband\win32
nmake -f NMakefile clean
nmake -f NMakefile BUILD=RELEASE
signtool sign /fd sha256 /n Mudfish /t http://timestamp.comodoca.com/authenticode mudband.exe
copy mudband.exe ..\..\..\dist\releases\%VERSION%\windows\x86\
copy mudband.pdb ..\..\..\dist\releases\%VERSION%\windows\x86\
cd ..\..\..\
copy contrib\prebuilt\wintun\bin\x86\wintun.dll dist\releases\%VERSION%\windows\x86\
cd dist\releases\%VERSION%\windows\x86\
7z a mudband-%VERSION%-windows-x86.zip mudband.exe mudband.pdb wintun.dll
