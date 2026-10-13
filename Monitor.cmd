@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if not errorlevel 1 (
    py -3 Monitoring\server.py %*
) else (
    python Monitoring\server.py %*
)
if errorlevel 1 pause
endlocal
