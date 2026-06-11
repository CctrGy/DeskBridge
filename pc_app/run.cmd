@echo off
setlocal

set "APP_DIR=%~dp0"
set "PYTHON=%APP_DIR%.venv\Scripts\python.exe"

if not exist "%PYTHON%" (
    set "PYTHON=python"
)

"%PYTHON%" "%APP_DIR%terminal.py" --cli %*
exit /b %ERRORLEVEL%
