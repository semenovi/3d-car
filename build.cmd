@echo off
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "CMAKE=cmake"
where cmake >nul 2>nul
if errorlevel 1 (
    set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if not exist "%CMAKE%" (
        echo [build] cmake not found on PATH and not found at the expected
        echo [build] Visual Studio bundled location. Install the Vulkan SDK
        echo [build] / Visual Studio C++ workload, or add cmake to PATH.
        exit /b 1
    )
)

"%CMAKE%" -S "%SCRIPT_DIR%" -B "%SCRIPT_DIR%\build"
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%SCRIPT_DIR%\build" --config %CONFIG%
if errorlevel 1 exit /b 1

if /i "%CONFIG%"=="Release" (
    for %%F in ("%SCRIPT_DIR%\build\Release\pickup_elite.exe") do (
        echo [build] pickup_elite.exe: %%~zF bytes
    )
)

exit /b 0
