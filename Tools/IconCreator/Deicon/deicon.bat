@echo off
REM Reverse of the .icon bake: decrypt *.icon -> .dds + .png
REM Usage: cd into a folder with *.icon files and run  ..\deicon.bat
REM    or: deicon.bat <folder>
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deicon.ps1" %*
