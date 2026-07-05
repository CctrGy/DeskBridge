@echo off
setlocal

set "ROOT_DIR=%~dp0"
set "APP_DIR=%ROOT_DIR%pc_app\"
set "PYTHON=%APP_DIR%.venv\Scripts\python.exe"

if not exist "%PYTHON%" (
    set "PYTHON=python"
)

"%PYTHON%" "%APP_DIR%terminal.py" %*
exit /b %ERRORLEVEL%
