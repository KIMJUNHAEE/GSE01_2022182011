@echo off
powershell -NoProfile -File "%~dp0Tools\Aseprite\OpenProtagonist.ps1"
if errorlevel 1 pause
