@echo off
rem Inject portable MSVC into the calling cmd process.
rem Keep this file CRLF and ASCII-only for cmd.exe.

if not defined PORTABLE_MSVC_ROOT set "PORTABLE_MSVC_ROOT=D:\Applications\msvc-portable\msvc"
if not exist "%PORTABLE_MSVC_ROOT%\setup_x64.bat" (
  call "%~dp0setup-msvc.bat" --yes
  if errorlevel 1 exit /b 1
)
call "%PORTABLE_MSVC_ROOT%\setup_x64.bat" >nul
if errorlevel 1 exit /b 1
where cl.exe >nul 2>nul || (
  echo ERROR: cl.exe was not found after MSVC setup. 1>&2
  exit /b 1
)
where link.exe >nul 2>nul || (
  echo ERROR: link.exe was not found after MSVC setup. 1>&2
  exit /b 1
)
exit /b 0
