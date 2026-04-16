@echo off
setlocal

echo Setting up build environment...

set VCPKG_DIR=%~dp0..\..\vcpkg\installed\x64-windows
set MSVC_DIR=C:\Program Files (x86)\Microsoft Visual Studio\2022
set SDK_DIR=C:\Program Files (x86)\Windows Kits\10

for /d %%D in ("%MSVC_DIR%\*\BuildTools\VC\Tools\MSVC\*") do set MSVC_VER=%%D
for /d %%D in ("%SDK_DIR%\Include\10.*") do set SDK_VER=%%~nxD

set CL_PATH=%MSVC_VER%\bin\Hostx64\x64
set INCLUDE=%MSVC_VER%\include
set INCLUDE=%INCLUDE%;%VCPKG_DIR%\include
set INCLUDE=%INCLUDE%;%SDK_DIR%\Include\%SDK_VER%\ucrt
set INCLUDE=%INCLUDE%;%SDK_DIR%\Include\%SDK_VER%\shared
set INCLUDE=%INCLUDE%;%SDK_DIR%\Include\%SDK_VER%\um
set INCLUDE=%INCLUDE%;%SDK_DIR%\Include\%SDK_VER%\winrt

set LIB=%MSVC_VER%\lib\onecore\x64
set LIB=%LIB%;%VCPKG_DIR%\lib
set LIB=%LIB%;%SDK_DIR%\Lib\%SDK_VER%\ucrt\x64
set LIB=%LIB%;%SDK_DIR%\Lib\%SDK_VER%\um\x64

set PATH=%CL_PATH%;%PATH%

if not exist "%~dp0..\libspeexdsp.dll" (
    echo Copying libspeexdsp.dll...
    copy "%VCPKG_DIR%\bin\libspeexdsp.dll" "%~dp0..\libspeexdsp.dll"
)

echo Building native library...
cl ..\engine.c /LD /O2 /D_UNICODE /DNATIVELIB_EXPORTS ^
 /I"%MSVC_VER%\include" ^
 /I"%VCPKG_DIR%\include" ^
 /I"%SDK_DIR%\Include\%SDK_VER%\ucrt" ^
 /I"%SDK_DIR%\Include\%SDK_VER%\shared" ^
 /I"%SDK_DIR%\Include\%SDK_VER%\um" ^
 /I"%SDK_DIR%\Include\%SDK_VER%\winrt" ^
 /Fe:NativeLib.dll ^
 /link /LIBPATH:"%MSVC_VER%\lib\onecore\x64" /LIBPATH:"%VCPKG_DIR%\lib" /LIBPATH:"%SDK_DIR%\Lib\%SDK_VER%\ucrt\x64" /LIBPATH:"%SDK_DIR%\Lib\%SDK_VER%\um\x64" ^
 speexdsp.lib user32.lib gdi32.lib comctl32.lib ole32.lib uuid.lib mmdevapi.lib shell32.lib uxtheme.lib dwmapi.lib

if %errorlevel% neq 0 (
    echo Build FAILED
    pause
    exit /b %errorlevel%
)

echo.
echo NativeLib.dll built successfully.

echo.
echo Publishing self-contained app...
dotnet publish "%~dp0SidetoneApp.csproj" -c Release -o "%~dp0publish"
if %errorlevel% neq 0 (
    echo Build FAILED
    pause
    exit /b %errorlevel%
)

echo.
echo Copying NativeLib.dll to publish folder...
copy /Y "%~dp0NativeLib.dll" "%~dp0publish\"
copy /Y "..\libspeexdsp.dll" "%~dp0publish\"

echo.
echo Build SUCCESS - Portable app in app\publish\
echo.
echo Files needed:
echo   - Sidetone.exe
echo   - NativeLib.dll
echo   - libspeexdsp.dll
