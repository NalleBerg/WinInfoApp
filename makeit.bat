@echo off
setlocal enabledelayedexpansion

rem Default generator and build type
set "CMAKE_GEN=MinGW Makefiles"
set "BUILD_TYPE=Release"

if not "%1"=="" (
    set "BUILD_TYPE=%1"
)

cls

rem Prefer MINGW64 installation at C:\mingw64 if present
if not defined MINGW_ROOT set "MINGW_ROOT=C:\mingw64"
if exist "%MINGW_ROOT%\bin" (
    set "OLD_PATH=%PATH%"
    set "PATH=%MINGW_ROOT%\bin;%PATH%"
)

rem choose make executable
set "MAKE_EXE=mingw32-make.exe"
if exist "%MINGW_ROOT%\bin\mingw32-make.exe" set "MAKE_EXE=%MINGW_ROOT%\bin\mingw32-make.exe"
if exist "%MINGW_ROOT%\bin\mingw64-make.exe" set "MAKE_EXE=%MINGW_ROOT%\bin\mingw64-make.exe"

rem Forcefully remove old build directory (including all contents)
if exist build (
    attrib -r -s -h build\*.* /s >nul 2>&1
    rmdir /s /q build >nul 2>&1
)

rem Configure the project for MinGW-w64
echo [INFO] Configuring with CMake generator: %CMAKE_GEN%
cmake -S . -B build -G "%CMAKE_GEN%"
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    endlocal
    exit /b 1
)

rem Build in chosen mode using detected make. Use parallel jobs when possible.
set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" set "JOBS=2"
echo [INFO] Building (jobs=%JOBS%) using %MAKE_EXE%
cmake --build build --config %BUILD_TYPE% -- -j%JOBS%
if errorlevel 1 (
    echo [ERROR] Build failed.
    endlocal
    exit /b 1
)

rem If Visual Studio layout was used rename Release to wininfoapp
if exist build\Release (
    ren build\Release wininfoapp
)

rem Determine executable path for MinGW (build\WinInfoApp.exe) or VS (build\wininfoapp\WinInfoApp.exe)
if exist build\WinInfoApp.exe (
    set "EXE_PATH=build\WinInfoApp.exe"
) else if exist build\wininfoapp\WinInfoApp.exe (
    set "EXE_PATH=build\wininfoapp\WinInfoApp.exe"
) else (
    echo [ERROR] Executable not found.
    endlocal
    exit /b 1
)

rem Run the executable
echo [INFO] Running %EXE_PATH%
"%EXE_PATH%"
if errorlevel 1 (
    echo [ERROR] Application failed to start.
    endlocal
    exit /b 1
)

rem restore PATH if modified
if defined OLD_PATH set "PATH=%OLD_PATH%"
endlocal