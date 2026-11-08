@echo off
setlocal
cd /d "%~dp0.."

call "%~dp0msvc-env.bat"
if errorlevel 1 exit /b 1

if not defined NAPPGUI_ROOT set "NAPPGUI_ROOT=%CD%\build\ui-native\nappgui-sdk-msvc-env"
if not exist "%NAPPGUI_ROOT%\Release\lib\osapp.lib" (
  echo ERROR: NAppGUI SDK not found at "%NAPPGUI_ROOT%". 1>&2
  echo Build it with NAPPGUI_DEMO=OFF and NAPPGUI_WEB=OFF first. 1>&2
  exit /b 1
)

set "BUILD_DIR=build\ui-native\desktop-nappgui-msvc"
cmake -S . -B "%BUILD_DIR%" -G Ninja ^
  -DAETHER_BUILD_NAPPGUI_FRONTEND=ON ^
  -DNAPPGUI_ROOT="%NAPPGUI_ROOT%" ^
  -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --target aether_frontend_nappgui -j 6
if errorlevel 1 exit /b 1

if not exist "build\ui-native\windows-demo" mkdir "build\ui-native\windows-demo"
copy /Y "%BUILD_DIR%\apps\desktop\nappgui\aether_frontend_nappgui.exe" ^
  "build\ui-native\windows-demo\aether_frontend_nappgui.exe" >nul
if errorlevel 1 exit /b 1

echo Built build\ui-native\windows-demo\aether_frontend_nappgui.exe
exit /b 0
