@echo off
setlocal
cd /d "%~dp0.."

call "%~dp0msvc-env.bat"
if errorlevel 1 exit /b 1

if /i "%~1"=="--clean" (
  if exist "build\msvc" rmdir /s /q "build\msvc"
)

cmake --preset msvc
if errorlevel 1 exit /b 1
cmake --build --preset msvc
if errorlevel 1 exit /b 1
ctest --preset msvc --output-on-failure
if errorlevel 1 exit /b 1
uv run --no-project python tools\check_code.py
if errorlevel 1 exit /b 1
build\msvc\aether_desktop.exe
exit /b %ERRORLEVEL%
