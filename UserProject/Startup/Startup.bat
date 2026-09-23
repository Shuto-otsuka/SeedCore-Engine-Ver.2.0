@echo off
chcp 65001 >nul

rem Everything this script says is written in Startup.ps1.
rem Japanese text is kept out of this file because cmd loses its place in a
rem UTF-8 batch file after a goto, which splits a later line into garbage.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Startup.ps1" %*

pause
