@echo off
setlocal

echo Setting up build environment...

set MSVC_DIR=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools
set SDK_DIR=C:\Program Files (x86)\Windows Kits\10
set VCPKG_DIR=C:\Users\Ryan\vcpkg\installed\x64-windows

set CL_PATH=%MSVC_DIR%\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64
set INCLUDE=%MSVC_DIR%\VC\Tools\MSVC\14.50.35717\include
set INCLUDE=%INCLUDE%;%VCPKG_DIR%\include
set INCLUDE=%INCLUDE%;%SDK_DIR%\Include\10.0.26100.0\ucrt
set INCLUDE=%INCLUDE%;%SDK_DIR%\Include\10.0.26100.0\shared
set INCLUDE=%INCLUDE%;%SDK_DIR%\Include\10.0.26100.0\um
set INCLUDE=%INCLUDE%;%SDK_DIR%\Include\10.0.26100.0\winrt

set LIB=%MSVC_DIR%\VC\Tools\MSVC\14.50.35717\lib\onecore\x64
set LIB=%LIB%;%VCPKG_DIR%\lib
set LIB=%LIB%;%SDK_DIR%\Lib\10.0.26100.0\ucrt\x64
set LIB=%LIB%;%SDK_DIR%\Lib\10.0.26100.0\um\x64

set PATH=%CL_PATH%;%PATH%

echo Building native library...
cl ..\engine.c /LD /O2 /D_UNICODE /DNATIVELIB_EXPORTS ^
 /I"%MSVC_DIR%\VC\Tools\MSVC\14.50.35717\include" ^
 /I"%VCPKG_DIR%\include" ^
 /I"%SDK_DIR%\Include\10.0.26100.0\ucrt" ^
 /I"%SDK_DIR%\Include\10.0.26100.0\shared" ^
 /I"%SDK_DIR%\Include\10.0.26100.0\um" ^
 /I"%SDK_DIR%\Include\10.0.26100.0\winrt" ^
 /Fe:NativeLib.dll ^
 /link /LIBPATH:"%MSVC_DIR%\VC\Tools\MSVC\14.50.35717\lib\onecore\x64" /LIBPATH:"%VCPKG_DIR%\lib" /LIBPATH:"%SDK_DIR%\Lib\10.0.26100.0\ucrt\x64" /LIBPATH:"%SDK_DIR%\Lib\10.0.26100.0\um\x64" ^
 speexdsp.lib user32.lib gdi32.lib comctl32.lib ole32.lib uuid.lib mmdevapi.lib shell32.lib uxtheme.lib dwmapi.lib

if %errorlevel% neq 0 (
    echo Build FAILED
    pause
    exit /b %errorlevel%
)

echo.
echo Build SUCCESS - NativeLib.dll

echo.
echo Building C# app...
dotnet build "%~dp0SidetoneApp.csproj" -c Release
if %errorlevel% neq 0 (
    echo Build FAILED
    pause
    exit /b %errorlevel%
)

echo.
echo Build SUCCESS
