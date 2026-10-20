@echo off
setlocal
cd /d "%~dp0"
echo Open http://127.0.0.1:8766/SpritePreview.html
where py >nul 2>nul
if not errorlevel 1 (
    py -3 -B -m http.server 8766 --bind 127.0.0.1 --directory "SimpleGame\Resources"
) else (
    python -B -m http.server 8766 --bind 127.0.0.1 --directory "SimpleGame\Resources"
)
if errorlevel 1 pause
endlocal
