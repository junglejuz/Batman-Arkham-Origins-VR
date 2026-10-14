@echo off
REM Batman Origins VR installer: runs Install-BatmanOriginsVR.ps1 from this folder.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-BatmanOriginsVR.ps1" %*
pause
