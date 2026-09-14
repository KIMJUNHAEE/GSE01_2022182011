@echo off
setlocal
if exist "%~dp0x64\Release\SimpleGame.exe" (
    start "" "%~dp0x64\Release\SimpleGame.exe"
    exit /b 0
)
if exist "%~dp0x64\Debug\SimpleGame.exe" (
    start "" "%~dp0x64\Debug\SimpleGame.exe"
    exit /b 0
)
echo Build SimpleGame.sln in Visual Studio with Release / x64 first.
pause
