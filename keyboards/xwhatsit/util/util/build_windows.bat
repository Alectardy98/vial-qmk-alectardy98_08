@echo off
setlocal
set "PATH=C:\Qt\6.8.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;%PATH%"

set "SCRIPT_DIR=%~dp0"
set "UTIL_ROOT=%SCRIPT_DIR%.."
set "BUILD_DIR=%UTIL_ROOT%\build-windows"
set "ZIP_FILE=%UTIL_ROOT%\Cap-Util-Windows-x64.zip"

echo.
echo === Building Cap-Util for Windows ===
echo.

cd /d "%SCRIPT_DIR%"

qmake util.pro CONFIG+=release
if errorlevel 1 exit /b 1

mingw32-make -j8
if errorlevel 1 exit /b 1

echo.
echo === Creating portable package ===
echo.

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

copy /Y "release\Cap-Util.exe" "%BUILD_DIR%\Cap-Util.exe" >nul
if errorlevel 1 exit /b 1

windeployqt --release --compiler-runtime "%BUILD_DIR%\Cap-Util.exe"
if errorlevel 1 exit /b 1

echo.
echo === Creating ZIP ===
echo.

if exist "%ZIP_FILE%" del /q "%ZIP_FILE%"

powershell -NoProfile -Command "Compress-Archive -Path '%BUILD_DIR%\*' -DestinationPath '%ZIP_FILE%' -Force"
if errorlevel 1 exit /b 1

echo.
echo ========================================
echo Build complete!
echo.
echo EXE folder:
echo %BUILD_DIR%
echo.
echo Portable ZIP:
echo %ZIP_FILE%
echo ========================================
echo.

endlocal