@echo off
REM Batman Origins VR - launcher through STEAMVR. Lives next to BatmanOrigins.exe.
REM The same as "Batman Origins VR.bat", but this one game launch uses SteamVR instead of the PC's
REM normal VR runtime. Nothing on the PC is changed. Needs: Steam running, SteamVR installed, the headset connected.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0OriginsVR\Launch-VR.ps1" -SteamVR
if errorlevel 1 pause
