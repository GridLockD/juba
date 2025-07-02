@echo off
setlocal

REM Adjust these paths if needed
set QMAKE=qmake
set MSBUILD=msbuild
set WINDEPLOYQT=windeployqt.exe
set ISCC=ISCC.exe

set BUILD_DIR=release_build

echo Building with qmake...
%QMAKE%
echo Building project...
%MSBUILD% /p:Configuration=Release /p:Platform=x64
if errorlevel 1 (
  echo Build failed.
  pause
  exit /b 1
)

mkdir "%BUILD_DIR%" 2>nul
copy Release\juba.exe %BUILD_DIR% >nul

echo Running windeployqt...
%WINDEPLOYQT% --release --compiler-runtime %BUILD_DIR%\juba.exe

echo Building installer with Inno Setup...
%ISCC% juba_installer.iss

echo Creating USB zip archive...
powershell Compress-Archive -Path .\windows\* -DestinationPath ..\juba_usb_distribution_win.zip -Force

echo Done.
pause
