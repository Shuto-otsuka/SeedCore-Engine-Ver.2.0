@echo off
chcp 65001 >nul
setlocal

:: 1. すでにインストールされているかチェック
where py >nul 2>&1
if %errorlevel% equ 0 (
    echo Pythonは既にインストールされています。
    py --version
    pause
    exit /b
)

:: 2. ユーザーに確認をとる
:PROMPT
set /p ANSWER="Pythonが検出されませんでした。インストールを開始しますか？ (Y/N): "

:: 入力を大文字に変換して比較 (Yならインストール、Nなら終了)
if /i "%ANSWER%"=="Y" goto INSTALL
if /i "%ANSWER%"=="N" exit /b

:: Y/N以外が入力されたらやり直し
echo Y または N を入力してください。
goto PROMPT

:INSTALL
echo インストーラーをダウンロード中...
set DOWNLOAD_URL=https://www.python.org/ftp/python/3.14.4/python-3.14.4-amd64.exe
set INSTALLER_NAME=python_installer.exe

powershell -Command "Invoke-WebRequest -Uri '%DOWNLOAD_URL%' -OutFile '%INSTALLER_NAME%'"

echo インストールを実行中...
start /wait "" %INSTALLER_NAME% /quiet InstallAllUsers=1 PrependPath=1 Include_pip=1

:: インストーラーの削除
if exist %INSTALLER_NAME% del %INSTALLER_NAME%

echo.
echo インストールが完了しました。
pause