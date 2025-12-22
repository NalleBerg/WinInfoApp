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

rem Ensure CMake is available
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found in PATH. Please install CMake.
    if defined OLD_PATH set "PATH=%OLD_PATH%"
    endlocal
    exit /b 1
)

rem Configure the project (pass CMAKE_BUILD_TYPE for single-config generators)
echo [INFO] Configuring with CMake generator: %CMAKE_GEN% (build type: %BUILD_TYPE%)
if ""=="%CMAKE_GEN%" (
    cmake -S . -B build -D CMAKE_BUILD_TYPE=%BUILD_TYPE%
) else (
    cmake -S . -B build -G "%CMAKE_GEN%" -D CMAKE_BUILD_TYPE=%BUILD_TYPE%
)
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    if defined OLD_PATH set "PATH=%OLD_PATH%"
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

rem Locate the built executable under the build directory
set "EXE_PATH="
for /r "build" %%F in (WinInfoApp.exe) do (
    set "EXE_PATH=%%~fF"
    goto :found_exe
)
:found_exe
if "%EXE_PATH%"=="" (
    echo [ERROR] Executable not found in build directory.
    if defined OLD_PATH set "PATH=%OLD_PATH%"
    endlocal
    exit /b 1
)

rem Run the executable
echo [INFO] Running %EXE_PATH%
"%EXE_PATH%"
if errorlevel 1 (
    echo [ERROR] Application failed to start.
    if defined OLD_PATH set "PATH=%OLD_PATH%"
    endlocal
    exit /b 1
)

rem restore PATH if modified
if defined OLD_PATH set "PATH=%OLD_PATH%"
endlocal