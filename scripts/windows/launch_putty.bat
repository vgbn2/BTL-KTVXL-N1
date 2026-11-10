@echo off
setlocal enabledelayedexpansion

set "TARGET_PORT="
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--help" goto usage
if /i "%~1"=="-h" goto usage
if /i "%~1"=="/?" goto usage
if /i "%~1"=="--port" (
    if "%~2"=="" goto bad_args
    set "TARGET_PORT=%~2"
    shift
    shift
    goto parse_args
)
if defined TARGET_PORT goto bad_args
set "TARGET_PORT=%~1"
shift
goto parse_args
:args_done

:: ============================================================================
:: launch_putty.bat - Automated PuTTY Launcher for STM32F103 UART Telemetry
:: Target: STM32F103C8T6 USART1 (115200 Baud, 8-N-1, No Flow Control)
:: ============================================================================

title STM32 Telemetry - PuTTY Launcher
echo ============================================================================
echo   STM32F103 USART1 Telemetry Console - PuTTY Automated Launcher
echo   Baud: 115200 | Data: 8 | Parity: None | Stop: 1 | Flow: None
echo ============================================================================
echo.

:: 1. Locate putty.exe
set "PUTTY_BIN="
where putty.exe >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "PUTTY_BIN=putty.exe"
) else if exist "%ProgramFiles%\PuTTY\putty.exe" (
    set "PUTTY_BIN=%ProgramFiles%\PuTTY\putty.exe"
) else if exist "%ProgramFiles(x86)%\PuTTY\putty.exe" (
    set "PUTTY_BIN=%ProgramFiles(x86)%\PuTTY\putty.exe"
) else if exist "%~dp0putty.exe" (
    set "PUTTY_BIN=%~dp0putty.exe"
) else (
    echo [ERROR] putty.exe was not found in PATH or standard installation directories.
    echo Please install PuTTY or place putty.exe in the same folder as this script.
    echo Download from: https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html
    echo.
    exit /b 1
)

echo [INFO] Found PuTTY: "!PUTTY_BIN!"
echo.

:: 2. Enumerate available COM ports via PowerShell
where powershell.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] powershell.exe is required to check available COM ports.
    exit /b 1
)
echo [INFO] Scanning active COM ports...
set "PORTS="
for /f "usebackq tokens=*" %%P in (`powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object"`) do (
    if defined PORTS (
        set "PORTS=!PORTS! %%P"
    ) else (
        set "PORTS=%%P"
    )
)

if "%PORTS%"=="" (
    echo [ERROR] No active COM ports detected by Windows.
    echo Ensure your USB-to-UART adapter (CH340/CP2102/FT232/ST-Link VCP) is plugged in.
    echo.
    exit /b 1
) else (
    echo [INFO] Detected COM ports: %PORTS%
    if defined TARGET_PORT goto validate_port
    
    :: Count ports
    set /a COUNT=0
    for %%P in (%PORTS%) do (
        set /a COUNT+=1
        set "LAST_PORT=%%P"
    )

    if !COUNT! equ 1 (
        echo [INFO] Auto-selected single available port: !LAST_PORT!
        set "TARGET_PORT=!LAST_PORT!"
    ) else (
        echo.
        echo Multiple COM ports available:
        set /a IDX=0
        for %%P in (%PORTS%) do (
            set /a IDX+=1
            echo   [!IDX!] %%P
        )
        echo.
        set /p "CHOICE=Select port number [1-!IDX!] or type COMx: "
        
        :: Test if numeric selection
        set "TARGET_PORT="
        set /a SEL_IDX=0
        for %%P in (%PORTS%) do (
            set /a SEL_IDX+=1
            if "!CHOICE!"=="!SEL_IDX!" set "TARGET_PORT=%%P"
        )
        if "!TARGET_PORT!"=="" set "TARGET_PORT=!CHOICE!"
    )
)

:validate_port
set "MATCHED_PORT="
for %%P in (%PORTS%) do (
    if /i "!TARGET_PORT!"=="%%P" set "MATCHED_PORT=%%P"
)
if not defined MATCHED_PORT (
    echo [ERROR] Selected COM port is not present. Use one of: %PORTS%
    exit /b 1
)
set "TARGET_PORT=!MATCHED_PORT!"

:: Sanitize port string (e.g. ensure uppercase "COM")
echo.
echo ============================================================================
echo   Launching PuTTY on %TARGET_PORT% @ 115200 8-N-1...
echo ============================================================================
echo.

start "" /wait "!PUTTY_BIN!" -serial "!TARGET_PORT!" -sercfg 115200,8,n,1,N
exit /b %ERRORLEVEL%

:usage
echo Usage: %~nx0 [COM3 ^| --port COM3]
echo PuTTY: 115200 baud, 8N1, no flow control. Without a port, discover and select.
exit /b 0

:bad_args
echo [ERROR] Expected one COM port or --port COM3. Use --help for usage.
exit /b 2
