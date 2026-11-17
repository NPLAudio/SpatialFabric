@echo off
cd /d "%~dp0"
py -3.13 launch.py %*
if errorlevel 1 pause
