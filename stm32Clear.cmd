@echo off
setlocal

set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=chip_core"

set "OPENOCD=%USERPROFILE%\.platformio\packages\tool-openocd\bin\openocd.exe"
set "OPENOCD_SCRIPTS=%USERPROFILE%\.platformio\packages\tool-openocd\openocd\scripts"
set "STM32_CLI=%ProgramFiles%\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

if /I "%TARGET%"=="chip_core" (
  set "TARGET_CFG=target/stm32f4x.cfg"
  set "FLASH_START=0x08000000"
  set "FLASH_SIZE=0x80000"
) else if /I "%TARGET%"=="core" (
  set "TARGET_CFG=target/stm32f4x.cfg"
  set "FLASH_START=0x08000000"
  set "FLASH_SIZE=0x80000"
) else if /I "%TARGET%"=="chip_keypad" (
  set "TARGET_CFG=target/stm32g0x.cfg"
  set "FLASH_START=0x08000000"
  set "FLASH_SIZE=0x8000"
) else if /I "%TARGET%"=="keypad" (
  set "TARGET_CFG=target/stm32g0x.cfg"
  set "FLASH_START=0x08000000"
  set "FLASH_SIZE=0x8000"
) else (
  echo Uso:
  echo   stm32Clear [chip_core^|chip_keypad]
  echo.
  echo Si no indicas objetivo, se usa chip_core.
  exit /b 1
)

echo Borrando memoria Flash de %TARGET% por ST-LINK/SWD...
echo.

if exist "%OPENOCD%" (
  call :erase_openocd "OpenOCD 400 kHz" 400
  if "%ERRORLEVEL%"=="0" goto :success

  call :erase_openocd "OpenOCD 100 kHz" 100
  if "%ERRORLEVEL%"=="0" goto :success
) else (
  echo OpenOCD de PlatformIO no encontrado en:
  echo "%OPENOCD%"
  echo.
)

if exist "%STM32_CLI%" (
  call :erase_cube "CubeProgrammer NORMAL 400 kHz" "port=SWD freq=400 mode=NORMAL reset=SWrst"
  if "%ERRORLEVEL%"=="0" goto :success

  call :erase_cube "CubeProgrammer HOTPLUG 400 kHz" "port=SWD freq=400 mode=HOTPLUG reset=SWrst"
  if "%ERRORLEVEL%"=="0" goto :success

  call :erase_cube "CubeProgrammer UNDER RESET 400 kHz" "port=SWD freq=400 mode=UR reset=HWrst"
  if "%ERRORLEVEL%"=="0" goto :success

  call :erase_cube "CubeProgrammer UNDER RESET 100 kHz" "port=SWD freq=100 mode=UR reset=HWrst"
  if "%ERRORLEVEL%"=="0" goto :success
) else (
  echo STM32CubeProgrammer no encontrado en:
  echo "%STM32_CLI%"
  echo.
)

echo.
echo Error durante el borrado.
echo.
echo Si falla tambien con OpenOCD, revisa GND comun, SWDIO, SWCLK, alimentacion y RST/NRST.
echo Para borrar bajo reset, conecta ST-LINK RST al RST/NRST del STM32.
exit /b 1

:success
echo.
echo STM32 borrado correctamente.
exit /b 0

:erase_openocd
set "LABEL=%~1"
set "SPEED=%~2"
echo Intentando %LABEL%...
"%OPENOCD%" -s "%OPENOCD_SCRIPTS%" -f interface/stlink.cfg -f "%TARGET_CFG%" -c "adapter speed %SPEED%" -c "init" -c "reset halt" -c "flash erase_address %FLASH_START% %FLASH_SIZE%" -c "reset run" -c "shutdown"
if "%ERRORLEVEL%"=="0" exit /b 0
echo Fallo con %LABEL%.
echo.
exit /b 1

:erase_cube
set "LABEL=%~1"
set "CONNECT_ARGS=%~2"
echo Intentando %LABEL%...
"%STM32_CLI%" -c %CONNECT_ARGS% -e all
if "%ERRORLEVEL%"=="0" exit /b 0
echo Fallo con %LABEL%.
echo.
exit /b 1
