@echo off
REM Batman Origins VR - launcher. Lives next to BatmanOrigins.exe (the installer puts it there).
REM Needs: Steam running and your PC VR software running (it must be the active OpenXR runtime).
REM OriginsVR\Launch-VR.ps1 asks the headset for its picture size, sets the game up for VR for this one launch
REM and puts your own graphics settings back when the game closes.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0OriginsVR\Launch-VR.ps1"
if errorlevel 1 pause
