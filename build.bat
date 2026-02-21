@echo off
REM BerkIDE Windows build script
REM BerkIDE Windows derleme betigi

setlocal

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
set BUILD_DIR=build

REM Configure if needed (incremental build support)
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
) else (
    REM Always reconfigure to pick up changes
    cmake -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
)

REM Build (incremental by default, use all cores)
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j %NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% neq 0 (
    echo Build FAILED.
    exit /b 1
)

echo Build complete (%BUILD_TYPE%). Binary: %BUILD_DIR%\%BUILD_TYPE%\berkide.exe

REM Copy runtime if exists
if exist ".berkide" (
    if not exist "%BUILD_DIR%\.berkide" (
        xcopy /E /I /Q ".berkide" "%BUILD_DIR%\.berkide"
        echo Copied .berkide runtime to build directory.
    )
)

endlocal
