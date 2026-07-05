@echo off
setlocal

set "ROOT=%~dp0"
set "TARGET=%~1"

if "%TARGET%"=="" (
  echo Uso:
  echo   stm32Programmer ^<carpeta_programa^>
  echo.
  echo Ejemplos:
  echo   stm32Programmer chip_core
  echo   stm32Programmer chip_keypad
  echo.
  echo Carpetas disponibles con platformio.ini:
  for /d %%D in ("%ROOT%*") do (
    if exist "%%~fD\platformio.ini" echo   %%~nxD
  )
  exit /b 1
)

set "PROJECT_DIR=%ROOT%%TARGET%"
if not exist "%PROJECT_DIR%\platformio.ini" (
  echo Proyecto no valido: "%TARGET%"
  echo No existe "%PROJECT_DIR%\platformio.ini"
  exit /b 1
)

set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
if not exist "%PIO%" (
  where pio >nul 2>nul
  if errorlevel 1 (
    echo PlatformIO no encontrado.
    echo No existe "%PIO%" y "pio" no esta en PATH.
    exit /b 1
  )
  set "PIO=pio"
)

echo Programando "%TARGET%" por ST-LINK...
echo Proyecto: "%PROJECT_DIR%"
echo.

"%PIO%" run -d "%PROJECT_DIR%" -t upload
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
  echo.
  echo Error programando "%TARGET%". Codigo: %RESULT%
  exit /b %RESULT%
)

echo.
echo "%TARGET%" programado correctamente por ST-LINK.
exit /b 0
