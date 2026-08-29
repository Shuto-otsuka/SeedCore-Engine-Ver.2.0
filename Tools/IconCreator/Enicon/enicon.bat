@echo off
REM image -> .icon : PNG/TGA -> BC7 DDS -> encrypted .icon container
REM Usage: cd into a folder with source images and run  ..\enicon.bat
REM    or: enicon.bat <folder>
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0enicon.ps1" %*
