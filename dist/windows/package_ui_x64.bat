set VERSION=0.1.0

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd ..
mkdir releases\%VERSION%\windows\x64\wix
cd ..\bin\mudband\win32
nmake -f NMakefile.x64 clean
nmake -f NMakefile.x64 BUILD=RELEASE
copy mudband.exe ..\..\..\dist\releases\%VERSION%\windows\x64\wix
copy mudband.pdb ..\..\..\dist\releases\%VERSION%\windows\x64\wix
cd ..\..\..\
copy contrib\prebuilt\wintun\bin\amd64\wintun.dll dist\releases\%VERSION%\windows\x64\wix
cd bin\mudband_service\windows
nmake -f NMakefile.x64 clean
nmake -f NMakefile.x64 BUILD=RELEASE
copy mudband_service.exe ..\..\..\dist\releases\%VERSION%\windows\x64\wix
copy mudband_service.pdb ..\..\..\dist\releases\%VERSION%\windows\x64\wix
cd ..\..\mudband_ui\windows
nmake -f NMakefile build64
copy .\src-tauri\target\x86_64-pc-windows-msvc\release\mudband_ui.exe ..\..\..\dist\releases\%VERSION%\windows\x64\wix
copy .\src-tauri\target\x86_64-pc-windows-msvc\release\mudband_ui.pdb ..\..\..\dist\releases\%VERSION%\windows\x64\wix

cd ..\..\..\dist\releases\%VERSION%\windows\x64\wix
signtool sign /fd sha256 /n Mudfish /t http://timestamp.comodoca.com/authenticode mudband.exe mudband_service.exe mudband_ui.exe

cd ..\..\..\..\..\windows\wix
dotnet build
signtool sign /fd sha256 /n Mudfish /t http://timestamp.comodoca.com/authenticode bin\x64\Debug\mudband.msi
copy bin\x64\Debug\mudband.msi ..\..\..\dist\releases\%VERSION%\windows\x64\mudband-ui-0.1.0-windows-x64.msi
copy bin\x64\Debug\mudband.wixpdb ..\..\..\dist\releases\%VERSION%\windows\x64\mudband-ui-0.1.0-windows-x64.wixpdb

