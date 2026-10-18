@echo off
setlocal
set PYTHON=python
set CMD=%1
if "%CMD%"=="" set CMD=build

if /i "%CMD%"=="build" (
    %PYTHON% setup.py build_ext --inplace
) else if /i "%CMD%"=="wheel" (
    %PYTHON% -m pip wheel . -w dist --no-deps
) else if /i "%CMD%"=="sdist" (
    %PYTHON% setup.py sdist
) else if /i "%CMD%"=="clean" (
    if exist build rmdir /s /q build
    if exist dist rmdir /s /q dist
    for /d %%d in (*.egg-info) do rmdir /s /q "%%d"
) else (
    echo Usage: build.bat [build^|wheel^|sdist^|clean]
    exit /b 1
)
exit /b %errorlevel%
