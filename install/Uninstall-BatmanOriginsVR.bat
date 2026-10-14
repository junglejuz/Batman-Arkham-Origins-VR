@echo off
REM Batman Origins VR uninstaller (lives in the game folder after installing).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Uninstall-BatmanOriginsVR.ps1" %*
pause
if not exist "%~dp0Uninstall-BatmanOriginsVR.ps1" (goto) 2>nul & del "%~f0"
