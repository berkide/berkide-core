@echo off
REM BerkIDE Windows build script
REM BerkIDE Windows derleme betigi

setlocal

set BUILD_TYPE=Release
set BUILD_DIR=build
set USE_TLS=OFF

REM Parse arguments
REM Argumanlari ayristir
for %%a in (%*) do (
    if "%%a"=="--tls" set USE_TLS=ON
    if "%%a"=="Debug" set BUILD_TYPE=Debug
    if "%%a"=="Release" set BUILD_TYPE=Release
    if "%%a"=="RelWithDebInfo" set BUILD_TYPE=RelWithDebInfo
    if "%%a"=="MinSizeRel" set BUILD_TYPE=MinSizeRel
)

REM Configure if needed (incremental build support)
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DBERKIDE_USE_TLS=%USE_TLS%
) else (
    REM Always reconfigure to pick up changes
    cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DBERKIDE_USE_TLS=%USE_TLS%
)

REM Build (incremental by default, use all cores)
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% neq 0 (
    echo Build FAILED.
    exit /b 1
)

echo Build complete (%BUILD_TYPE%, TLS=%USE_TLS%). Binary: %BUILD_DIR%\%BUILD_TYPE%\berkide.exe

REM Sync runtime to build directory (always update, not just first time)
REM Runtime dosyalarini build dizinine senkronize et (her zaman guncelle)
if exist ".berkide" (
    if exist "%BUILD_DIR%\.berkide" rmdir /S /Q "%BUILD_DIR%\.berkide"
    xcopy /E /I /Q ".berkide" "%BUILD_DIR%\.berkide"
    echo Synced .berkide runtime to build directory.
)

endlocal
