@echo off
setlocal

set "EXE=%~1"
if "%EXE%"=="" (
    echo [pack_release] no exe path given, nothing to do.
    exit /b 0
)
if not exist "%EXE%" (
    echo [pack_release] "%EXE%" does not exist, nothing to do.
    exit /b 0
)

where upx >nul 2>nul
if errorlevel 1 (
    echo [pack_release] upx not found on PATH - leaving "%EXE%" unpacked.
    echo [pack_release] install with: winget install --id UPX.UPX
    exit /b 0
)

rem Idempotent: if the exe is already UPX-packed (e.g. an unchanged
rem incremental rebuild re-running this step), decompress it first so
rem packing below never fails with "already packed". Ignore failure here -
rem it fails harmlessly if the exe was never packed to begin with.
upx -q -d "%EXE%" >nul 2>nul

upx -q --best --lzma "%EXE%"
if errorlevel 1 (
    echo [pack_release] upx failed to pack "%EXE%" - leaving it as-is.
    exit /b 0
)

exit /b 0
