@echo off
setlocal

set "CMAKE_GEN=Visual Studio 17 2022"
set "BUILD_TYPE=Release"

if not "%1"=="" (
    set "BUILD_TYPE=%1"
)

cls

REM Forcefully remove old build directory (including all contents)
if exist build (
    attrib -r -s -h build\*.* /s
    rmdir /s /q build
)

REM Configure the project for Visual Studio x64
cmake -S . -B build -G "%CMAKE_GEN%" -A x64
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

REM Build in chosen mode
cmake --build build --config %BUILD_TYPE% --verbose
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

REM Rename build\Release to build\wininfoapp if it exists
if exist build\Release (
    ren build\Release wininfoapp
)

REM Run the app from the new folder
if exist build\wininfoapp\WinInfoApp.exe (
    .\build\wininfoapp\WinInfoApp.exe
    if errorlevel 1 (
        echo [ERROR] Application failed to start.
        exit /b 1
    )
) else (
    echo [ERROR] Executable not found in build\wininfoapp.
    exit /b 1
)


REM Copy .\wininfoapp.ico .\build\wininfoapp\wininfoapp.ico
REM Copy .\wininfoapp.png .\build\wininfoapp\wininfoapp.png
endlocal