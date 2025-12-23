@echo off
setlocal enabledelayedexpansion

rem Default generator and build type
set "CMAKE_GEN=MinGW Makefiles"
set "BUILD_TYPE=Release"

rem Parse args: non-flag first/last arg is build type; --pkg enables packaging; --nsis enables NSIS build
set "DO_PKG=0"
set "DO_NSIS=0"
for %%A in (%*) do (
    if "%%~A"=="--pkg" (
        set "DO_PKG=1"
    ) else if "%%~A"=="--nsis" (
        set "DO_NSIS=1"
    ) else (
        set "BUILD_TYPE=%%~A"
    )
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

rem Perform a clean build: remove previous build directory to ensure fresh build
if exist "build" (
    echo [INFO] Removing previous build directory for clean build...
    rd /s /q "build"
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

rem Package the built files into a deployable folder `WinInfoApp`
set "PKG_DIR=%CD%\WinInfoApp"
echo [INFO] Creating package directory (preserved between runs): %PKG_DIR%
if not exist "%PKG_DIR%" mkdir "%PKG_DIR%" >nul 2>&1
if not exist "%PKG_DIR%\dll" mkdir "%PKG_DIR%\dll" >nul 2>&1

echo [INFO] Files that would be copied (dry-run):
echo - WinInfoApp.exe
robocopy "build" "%PKG_DIR%" "WinInfoApp.exe" /L /NJH /NJS /NDL >nul 2>&1 || echo [WARN] robocopy list failed or produced no output
echo - libs/*.dll -> %PKG_DIR%\dll\
robocopy "%CD%\libs" "%PKG_DIR%\dll" *.dll /L /NJH /NJS /NDL >nul 2>&1 || echo [WARN] robocopy list failed or produced no output
echo - img, resources, i18n
robocopy "%CD%\img" "%PKG_DIR%\img" /E /L /NJH /NJS /NDL >nul 2>&1 || echo [INFO] no img or robocopy not available
robocopy "%CD%\resources" "%PKG_DIR%\resources" /E /L /NJH /NJS /NDL >nul 2>&1 || echo [INFO] no resources or robocopy not available
robocopy "%CD%\i18n" "%PKG_DIR%\i18n" /E /L /NJH /NJS /NDL >nul 2>&1 || echo [INFO] no i18n or robocopy not available


echo [INFO] Auto-confirmed packaging; proceeding to copy newer/changed files into %PKG_DIR%...

echo [INFO] Copying WinInfoApp.exe to package (only newer will be copied)...
robocopy "build" "%PKG_DIR%" "WinInfoApp.exe" /NJH /NJS /NDL /XO >nul 2>&1 || xcopy /Y /D "build\WinInfoApp.exe" "%PKG_DIR%\" >nul 2>&1

echo [INFO] Copying DLLs to %PKG_DIR%\dll\ (only newer/changed files)...
robocopy "%CD%\libs" "%PKG_DIR%\dll" *.dll /E /NJH /NJS /NDL /XO >nul 2>&1 || (
    echo [INFO] robocopy failed, falling back to xcopy for DLLs (only newer)
    xcopy "%CD%\libs\*.dll" "%PKG_DIR%\dll\" /Y /D >nul 2>&1
)

echo [INFO] Copying optional assets (img, resources, i18n)...
robocopy "%CD%\img" "%PKG_DIR%\img" /E /NJH /NJS /NDL /XO >nul 2>&1 || echo [INFO] img not copied
robocopy "%CD%\resources" "%PKG_DIR%\resources" /E /NJH /NJS /NDL /XO >nul 2>&1 || echo [INFO] resources not copied
robocopy "%CD%\i18n" "%PKG_DIR%\i18n" /E /NJH /NJS /NDL /XO >nul 2>&1 || echo [INFO] i18n not copied
rem sqlite3 folder not copied by default to reduce package size; add manually if needed

echo [INFO] Package created/updated at %PKG_DIR%
echo [INFO] Done. The script will not run the app; run it yourself from the package directory.
if defined OLD_PATH set "PATH=%OLD_PATH%"
rem --- Attempt to include MSVC runtimes required by wrapper DLLs ---
rem Only copy from system folders when present; prefer 64-bit System32 for x64 build
set "SYS32=%WINDIR%\System32"
set "SYSWOW=%WINDIR%\SysWOW64"
echo [INFO] Including MSVC runtime DLLs into %PKG_DIR%\dll if available...
setlocal enabledelayedexpansion
set "RUNTIME_DLLS=VCRUNTIME140.dll MSVCP140.dll ucrtbase.dll"
for %%D in (%RUNTIME_DLLS%) do (
    if exist "%SYS32%\%%D" (
        echo [INFO] Copying %%D from %SYS32% to %PKG_DIR%\dll\
        copy /Y "%SYS32%\%%D" "%PKG_DIR%\dll\" >nul 2>&1
    ) else if exist "%SYSWOW%\%%D" (
        echo [INFO] Copying %%D from %SYSWOW% to %PKG_DIR%\dll\
        copy /Y "%SYSWOW%\%%D" "%PKG_DIR%\dll\" >nul 2>&1
    ) else (
        echo [WARN] %%D not found on this system; installer may still require VC++ Redistributable
    )
)

rem Copy api-ms-win-crt-* set if present (UCRT forwarders)
for %%F in ("%SYS32%\api-ms-win-crt-*.dll") do (
    if exist %%F (
        echo [INFO] Copying api-crt %%~nF to %PKG_DIR%\dll\
        copy /Y "%%F" "%PKG_DIR%\dll\" >nul 2>&1
    )
)
endlocal

rem --- If requested, produce a simple test installer package in ./pkg ---
if "%DO_PKG%"=="1" (
    echo [INFO] --pkg requested: creating pkg directory and building Setup stub
    set "OUTROOT=%CD%\pkg"
    if not exist "%OUTROOT%" mkdir "%OUTROOT%" >nul 2>&1
    rem Ensure package dir exists (safety) and pkg output dir exists
    if not exist "%PKG_DIR%" (
        echo [INFO] Creating missing package directory: %PKG_DIR%
        mkdir "%PKG_DIR%" >nul 2>&1
        mkdir "%PKG_DIR%\dll" >nul 2>&1
    )
    if not exist "%OUTROOT%" (
        echo [INFO] Creating output pkg directory: %OUTROOT%
        mkdir "%OUTROOT%" >nul 2>&1
    )

    rem copy license logo into package root for display
    if exist "%CD%\WinInfoApp\GPLv2.Logo.png" (
        copy /Y "%CD%\WinInfoApp\GPLv2.Logo.png" "%PKG_DIR%\" >nul 2>&1
    )

    rem create a prettier license markdown that includes the logo and the GPLv2 text
    set "LICENSE_SRC=%CD%\WinInfoApp\GPLv2.md"
    set "LICENSE_OUT=%PKG_DIR%\LICENSE_DISPLAY.md"
    if exist "%LICENSE_SRC%" (
        rem write image markdown first (escape ! with ^ to avoid delayed-expansion issues)
        echo ^![GPLv2 Logo](GPLv2.Logo.png) > "%LICENSE_OUT%"
        echo. >> "%LICENSE_OUT%"
        type "%LICENSE_SRC%" >> "%LICENSE_OUT%"
    )

    rem Determine version from source (wininfoapp.cpp) via PowerShell regex
    set "VERSION=0.0.0"
    rem extract version via PowerShell into a temp file, then read it
    powershell -NoProfile -Command "$line=(Select-String -Path '%CD%\wininfoapp.cpp' -Pattern 'versionnumber' | Select-Object -First 1).Line; if ($line -match '\"([0-9]+(\.[0-9]+)+)\"') { Write-Output $matches[1] }" > "%TEMP%\wininfo_ver.txt" 2>nul
    if exist "%TEMP%\wininfo_ver.txt" (
        set /p VERSION=<"%TEMP%\wininfo_ver.txt"
        del "%TEMP%\wininfo_ver.txt" >nul 2>&1
    )

    if "%VERSION%"=="" set "VERSION=0.0.0"
    set "OUTNAME=WinInfoApp-V%VERSION%-Setup"

    rem If target filename already exists, append incremental suffixes using a safe internal name (hyphen suffix)
    set "INTERNAL_FINALNAME=%OUTNAME%"
    set /a "PKG_IDX=1"
    :pkg_name_check
    if exist "%OUTROOT%\!INTERNAL_FINALNAME!.exe" (
        set "INTERNAL_FINALNAME=%OUTNAME%-%PKG_IDX%"
        set /a PKG_IDX+=1
        goto :pkg_name_check
    )

    rem Create a zip archive of the package from inside the package folder to avoid path interpretation issues
    echo [INFO] Creating archive %OUTROOT%\!INTERNAL_FINALNAME!.exe (zip renamed to exe)...
    powershell -NoProfile -Command "if (-not (Test-Path '%PKG_DIR%')) { throw 'Source package directory not found' } ; if (-not (Test-Path '%OUTROOT%')) { New-Item -ItemType Directory -Path '%OUTROOT%' | Out-Null } ; Compress-Archive -Path '%PKG_DIR%\*' -DestinationPath '%OUTROOT%\!INTERNAL_FINALNAME!.zip' -Force" >nul 2>&1
    if exist "%OUTROOT%\!INTERNAL_FINALNAME!.zip" (
        move /Y "%OUTROOT%\!INTERNAL_FINALNAME!.zip" "%OUTROOT%\!INTERNAL_FINALNAME!.exe" >nul 2>nul
        if exist "%OUTROOT%\!INTERNAL_FINALNAME!.exe" (
            echo [INFO] Package written to %OUTROOT%\!INTERNAL_FINALNAME!.exe
        ) else (
            echo [WARN] Archive was created but could not be renamed to .exe
        )
    ) else (
        echo [WARN] Archive creation failed; no pkg produced
    )
)

rem If requested, attempt to build NSIS installer using the packaged folder (run outside the inner parenthesis)
if "%DO_NSIS%"=="1" (
    echo [INFO] --nsis requested: attempting automated NSIS build if makensis is available...
    where makensis >nul 2>&1
    if errorlevel 1 (
        echo [INFO] makensis not found in PATH — proceeding to download NSIS portable and run makensis automatically (assume Yes).
        rem Prefer a bundled portable NSIS checked into the repo at tools\nsis_portable.
        rem If not present, fall back to downloading the portable zip into %TEMP%.
        set "BUNDLED_NSIS=%CD%\tools\nsis_portable"
        if exist "%BUNDLED_NSIS%" (
            echo [INFO] Found bundled NSIS at %BUNDLED_NSIS% — using that instead of downloading.
            set "NSIS_DIR=%BUNDLED_NSIS%"
        ) else (
            rem Use portable NSIS (no admin). Download zip to temp and extract.
            set "NSIS_ZIP=%TEMP%\nsis_portable.zip"
            set "NSIS_DIR=%TEMP%\nsis_portable"
            if exist "%NSIS_DIR%" rd /s /q "%NSIS_DIR%" >nul 2>&1
            echo [INFO] Bundled NSIS not found; downloading NSIS portable zip to %NSIS_ZIP% ...
            powershell -NoProfile -Command "try { Invoke-WebRequest -Uri 'https://prdownloads.sourceforge.net/nsis/nsis-3.08.1-portable.zip' -OutFile '%NSIS_ZIP%' -UseBasicParsing -ErrorAction Stop } catch { exit 1 }" >nul 2>&1
            if exist "%NSIS_ZIP%" (
                echo [INFO] Extracting NSIS portable to %NSIS_DIR% ...
                powershell -NoProfile -Command "try { Expand-Archive -LiteralPath '%NSIS_ZIP%' -DestinationPath '%NSIS_DIR%' -Force } catch { exit 1 }" >nul 2>&1
                del "%NSIS_ZIP%" >nul 2>&1
            ) else (
                echo [ERROR] Failed to download NSIS portable zip; cannot build installer automatically
            )
        )

        rem locate makensis in NSIS_DIR (either bundled or downloaded)
        if exist "%NSIS_DIR%" (
            set "NSIS_EXE="
            for /r "%NSIS_DIR%" %%M in (makensis.exe) do (
                set "NSIS_EXE=%%~fM"
                goto :_nsis_found
            )
            :_nsis_found
            if defined NSIS_EXE (
                echo [INFO] Found makensis at !NSIS_EXE! — running installer build now.
                set "NSIS_SCRIPT=%CD%\WinInfoApp.nsi"
                if not exist "%PKG_DIR%" (
                    echo [ERROR] Package directory %PKG_DIR% missing; run with --pkg first
                ) else if not exist "%NSIS_SCRIPT%" (
                    echo [ERROR] NSIS script %NSIS_SCRIPT% not found; cannot build installer
                ) else (
                    pushd "%PKG_DIR%" >nul 2>&1
                    "!NSIS_EXE!" -V2 -DOUTNAME="!INTERNAL_FINALNAME!" "%NSIS_SCRIPT%"
                    if errorlevel 1 (
                        echo [WARN] makensis reported errors; installer may not have been created
                    ) else (
                        echo [INFO] makensis completed; installer should be available in %PKG_DIR%
                    )
                    popd >nul 2>&1
                )
            ) else (
                echo [ERROR] makensis not found inside NSIS directory (%NSIS_DIR%); aborting automated NSIS build
            )
        ) else (
            echo [ERROR] NSIS directory %NSIS_DIR% not present; cannot run makensis
        )
    ) else (
        rem Prefer internal hyphen-suffixed name (no parentheses) to avoid cmd parsing issues
        if "!INTERNAL_FINALNAME!"=="" (
            set "NSIS_OUTNAME=%OUTNAME%"
        ) else (
            set "NSIS_OUTNAME=!INTERNAL_FINALNAME!"
        )
        rem Build NSIS installer from package folder using a temporary helper script to avoid nested-paren parsing issues
        if not exist "%PKG_DIR%" (
            echo [ERROR] Package directory %PKG_DIR% missing; run with --pkg first
        ) else (
            set "NSIS_SCRIPT=%CD%\WinInfoApp.nsi"
            if not exist "%NSIS_SCRIPT%" (
                echo [ERROR] NSIS script %NSIS_SCRIPT% not found; cannot build installer
            ) else (
                echo [INFO] Running makensis via temporary PowerShell helper (safer)
                set "TMP_PS1=%TEMP%\wininfo_makensis.ps1"
                >"%TMP_PS1%" echo param([string]$pkgdir,[string]$outname,[string]$nsis)
                >>"%TMP_PS1%" echo Set-Location -LiteralPath $pkgdir
                >>"%TMP_PS1%" echo $psi = New-Object System.Diagnostics.ProcessStartInfo
                >>"%TMP_PS1%" echo $psi.FileName = 'makensis'
                >>"%TMP_PS1%" echo $psi.Arguments = "-V2 -DOUTNAME=`"$outname`" `"$nsis`""
                >>"%TMP_PS1%" echo $psi.UseShellExecute = $false
                >>"%TMP_PS1%" echo $p = [System.Diagnostics.Process]::Start($psi)
                >>"%TMP_PS1%" echo $p.WaitForExit()
                powershell -NoProfile -ExecutionPolicy Bypass -File "%TMP_PS1%" -pkgdir "%PKG_DIR%" -outname "!NSIS_OUTNAME!" -nsis "%NSIS_SCRIPT%"
                if errorlevel 1 (
                    echo [WARN] makensis reported errors or PowerShell failed; installer may not have been created
                ) else (
                    echo [INFO] makensis (via PowerShell) completed; check %PKG_DIR% for the generated installer
                )
                del "%TMP_PS1%" >nul 2>&1
            )
        )
    )
)

exit /b 0