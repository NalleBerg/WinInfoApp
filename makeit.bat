@echo off
setlocal

REM === User-configurable paths ===
set "QT_BIN=C:\Qt\6.9.1\msvc2022_64\bin"
set "CMAKE_GEN=Visual Studio 17 2022"
set "BUILD_TYPE=Release"

REM === Optional: allow build type as argument ===
if not "%1"=="" (
    set "BUILD_TYPE=%1"
)

cls

REM Remove old build directory
rmdir /s /q build

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

REM Deploy Qt dependencies
"%QT_BIN%\windeployqt.exe" .\build\%BUILD_TYPE%\wininfoapp.exe
if errorlevel 1 (
    echo [ERROR] windeployqt failed.
    exit /b 1
)

REM Rename the build directory
ren .\build\%BUILD_TYPE% wininfoapp
if errorlevel 1 (
    echo [ERROR] Rename failed.
    exit /b 1
)

REM Run the app
.\build\wininfoapp\wininfoapp.exe
if errorlevel 1 (
    echo [ERROR] Application failed to start.
    exit /b 1
)

endlocal