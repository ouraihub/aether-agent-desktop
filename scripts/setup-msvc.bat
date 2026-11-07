@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

rem Download the shared portable MSVC toolchain when it is missing.
rem Keep this file CRLF and ASCII-only for cmd.exe.

if not defined PORTABLE_MSVC_ROOT set "PORTABLE_MSVC_ROOT=D:\Applications\msvc-portable\msvc"
set "MSVC_VERSION=14.44"
set "SDK_VERSION=26100"
set "GIST_URL=https://gist.githubusercontent.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977/raw/portable-msvc.py"
set "YES="
for %%F in (%*) do (
  if /i "%%~F"=="--yes" set "YES=1"
  if /i "%%~F"=="-y" set "YES=1"
  if /i "%%~F"=="--help" goto usage
  if /i "%%~F"=="-h" goto usage
)

if exist "%PORTABLE_MSVC_ROOT%\setup_x64.bat" (
  echo Portable MSVC already exists: %PORTABLE_MSVC_ROOT%
  exit /b 0
)

where uv >nul 2>nul || goto no_uv
where curl >nul 2>nul || goto no_curl
for %%P in ("%PORTABLE_MSVC_ROOT%\..") do set "WORKDIR=%%~fP"
if not exist "%WORKDIR%" mkdir "%WORKDIR%" || goto no_workdir

echo This downloads about 455 MB of Microsoft build components to:
echo   %WORKDIR%\msvc
echo The components are Microsoft's and their licence terms apply.
if defined YES goto confirmed
choice /c YN /n /m "Proceed? [Y/N] "
if errorlevel 2 goto aborted

:confirmed
echo Fetching portable-msvc.py ...
curl -fsSL "%GIST_URL%" -o "%WORKDIR%\portable-msvc.py" || goto download_failed
pushd "%WORKDIR%"
uv run --python 3.12 python portable-msvc.py --accept-license --msvc-version %MSVC_VERSION% --sdk-version %SDK_VERSION% --target x64 --host x64
set "RC=%ERRORLEVEL%"
popd
if not "%RC%"=="0" goto install_failed
if not exist "%PORTABLE_MSVC_ROOT%\setup_x64.bat" goto incomplete
echo Portable MSVC installed: %PORTABLE_MSVC_ROOT%
exit /b 0

:no_uv
echo ERROR: uv is required to download the portable MSVC toolchain. 1>&2
echo Install uv from https://docs.astral.sh/uv/ and reopen the terminal. 1>&2
exit /b 1
:no_curl
echo ERROR: curl is required. Windows 10 1803 and later normally include it. 1>&2
exit /b 1
:no_workdir
echo ERROR: cannot create %WORKDIR%. 1>&2
exit /b 1
:download_failed
echo ERROR: cannot download portable-msvc.py. 1>&2
exit /b 1
:install_failed
echo ERROR: portable MSVC download failed with code %RC%. 1>&2
exit /b %RC%
:incomplete
echo ERROR: setup_x64.bat is missing after download. 1>&2
exit /b 1
:aborted
echo Aborted. Nothing was downloaded.
exit /b 1
:usage
echo Usage: scripts\setup-msvc.bat [--yes]
echo Downloads MSVC 14.44 and Windows SDK 26100 for x64.
echo Set PORTABLE_MSVC_ROOT to install somewhere else.
exit /b 0
