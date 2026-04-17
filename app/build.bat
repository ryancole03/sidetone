@echo off
setlocal

echo Setting up build environment...

taskkill /F /IM Sidetone.exe 2>nul

if defined VCPKG_ROOT (
    set "VCPKG_DIR=%VCPKG_ROOT%\installed\x64-windows"
) else if exist "%USERPROFILE%\vcpkg\installed\x64-windows" (
    set "VCPKG_DIR=%USERPROFILE%\vcpkg\installed\x64-windows"
) else (
    for %%D in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
        if exist "%%D:\vcpkg\installed\x64-windows" set "VCPKG_DIR=%%D:\vcpkg\installed\x64-windows"
    )
)
set "SDK_DIR=C:\Program Files (x86)\Windows Kits\10"

for %%D in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    for /d %%V in ("%%D:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*") do set "MSVC_VER=%%V"
    for /d %%V in ("%%D:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\*") do set "MSVC_VER_FALLBACK=%%V"
    for /d %%V in ("%%D:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\*") do set "MSVC_VER_FALLBACK=%%V"
    for /d %%S in ("%%D:\Program Files (x86)\Windows Kits\10\Include\10.*") do set "SDK_VER=%%~nxS"
    if exist "%%D:\Program Files (x86)\Windows Kits\10" set "SDK_DIR=%%D:\Program Files (x86)\Windows Kits\10"
)

if not defined MSVC_VER (
    if defined MSVC_VER_FALLBACK (
        set "MSVC_VER=%MSVC_VER_FALLBACK%"
    )
)

if not defined MSVC_VER (
    echo ERROR: MSVC not found. Install Visual Studio Build Tools 2019 or 2022.
    exit /b 1
)

if not defined SDK_VER (
    echo ERROR: Windows SDK not found.
    exit /b 1
)

set "CL_PATH=%MSVC_VER%\bin\Hostx64\x64"
set "INCLUDE=%MSVC_VER%\include"
set "INCLUDE=%INCLUDE%;%VCPKG_DIR%\include"
set "INCLUDE=%INCLUDE%;%SDK_DIR%\Include\%SDK_VER%\ucrt"
set "INCLUDE=%INCLUDE%;%SDK_DIR%\Include\%SDK_VER%\shared"
set "INCLUDE=%INCLUDE%;%SDK_DIR%\Include\%SDK_VER%\um"
set "INCLUDE=%INCLUDE%;%SDK_DIR%\Include\%SDK_VER%\winrt"

set "LIB=%MSVC_VER%\lib\onecore\x64"
set "LIB=%LIB%;%VCPKG_DIR%\lib"
set "LIB=%LIB%;%SDK_DIR%\Lib\%SDK_VER%\ucrt\x64"
set "LIB=%LIB%;%SDK_DIR%\Lib\%SDK_VER%\um\x64"

set "PATH=%CL_PATH%;%PATH%"

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