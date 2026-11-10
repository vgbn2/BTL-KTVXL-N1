<#
.SYNOPSIS
    serial_terminal.ps1 - Native Windows PowerShell Zero-Install UART Terminal
.DESCRIPTION
    Connects to STM32F103 USART1 telemetry interface via System.IO.Ports.SerialPort.
    No external software (PuTTY / Python) required. Runs on all standard Windows 10/11 installations.
.PARAMETER Port
    COM port identifier (e.g. 'COM3'). If omitted, available ports are auto-discovered.
.PARAMETER BaudRate
    Baud rate (default: 115200).
.EXAMPLE
    .\serial_terminal.ps1
    .\serial_terminal.ps1 -Port COM3
    .\serial_terminal.ps1 -Port COM4 -BaudRate 115200
#>

[CmdletBinding()]
param (
    [string]$Port = "",
    [int]$BaudRate = 115200
)

# Set UTF-8 encoding
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "  STM32F103 UART Telemetry Terminal (PowerShell Stdlib .NET SerialPort)   " -ForegroundColor Cyan
Write-Host "  Framing: 115200 8-N-1 | Delimiter: CRLF (\r\n)                            " -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host ""

# 1. Enumerate available ports if not provided
if ([string]::IsNullOrWhiteSpace($Port)) {
    $availablePorts = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    if (-not $availablePorts -or $availablePorts.Count -eq 0) {
        Write-Host "[ERROR] No active COM ports detected by Windows." -ForegroundColor Red
        Write-Host "Please plug in your USB-to-UART adapter (CH340, CP2102, FT232, ST-Link VCP)."
        exit 1
    }

    if ($availablePorts.Count -eq 1) {
        $Port = $availablePorts[0]
        Write-Host "[INFO] Auto-selected single available port: $Port" -ForegroundColor Green
    } else {
        Write-Host "Multiple COM ports detected:" -ForegroundColor Yellow
        for ($i = 0; $i -lt $availablePorts.Count; $i++) {
            Write-Host ("  [{0}] {1}" -f ($i + 1), $availablePorts[$i])
        }
        $selection = Read-Host "Select port number [1-$($availablePorts.Count)] or enter COM name"
        if ($selection -match '^\d+$' -and [int]$selection -ge 1 -and [int]$selection -le $availablePorts.Count) {
            $Port = $availablePorts[[int]$selection - 1]
        } else {
            $Port = $selection.Trim().ToUpper()
        }
    }
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    Write-Host "[ERROR] Invalid port selection." -ForegroundColor Red
    exit 1
}

# 2. Instantiate and configure SerialPort
$serial = New-Object System.IO.Ports.SerialPort
$serial.PortName = $Port
$serial.BaudRate = $BaudRate
$serial.DataBits = 8
$serial.Parity = [System.IO.Ports.Parity]::None
$serial.StopBits = [System.IO.Ports.StopBits]::One
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.ReadTimeout = 300
$serial.WriteTimeout = 1000
$serial.NewLine = "`r`n"

try {
    Write-Host "[INFO] Opening $Port at $BaudRate baud..." -ForegroundColor Gray
    $serial.Open()
    Write-Host "[CONNECTED] Port $Port opened successfully." -ForegroundColor Green
    Write-Host ""
} catch {
    Write-Host "[ERROR] Failed to open port $Port : $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Ensure the port is not open in another tool (PuTTY, Arduino IDE, Hercules, etc.)."
    exit 1
}

function Read-IncomingBuffer {
    param ($portObj)
    try {
        while ($portObj.IsOpen -and $portObj.BytesToRead -gt 0) {
            $line = $portObj.ReadLine()
            if (-not [string]::IsNullOrEmpty($line)) {
                $timestamp = (Get-Date).ToString("HH:mm:ss.fff")
                Write-Host "[$timestamp RX] $line" -ForegroundColor Green
            }
        }
    } catch [System.TimeoutException] {
        # Normal timeout when no more lines available
    } catch {
        # Other read errors
    }
}

function Send-Command {
    param ($portObj, [string]$cmd)
    try {
        Read-IncomingBuffer -portObj $portObj
        $timestamp = (Get-Date).ToString("HH:mm:ss.fff")
        Write-Host "[$timestamp TX] $cmd" -ForegroundColor Yellow
        $portObj.WriteLine($cmd)
        
        # Allow MCU time to process and respond
        Start-Sleep -Milliseconds 150
        Read-IncomingBuffer -portObj $portObj
    } catch {
        Write-Host "[ERROR TX] Failed to send: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Interactive loop
try {
    Write-Host "Available quick commands:" -ForegroundColor Cyan
    Write-Host "  [1] INFO_REQUEST         - Query firmware, group & project info"
    Write-Host "  [2] GET_ANGLE            - Read instantaneous Roll & Pitch angles"
    Write-Host "  [3] CALIBRATE_IMU        - Run static IMU calibration & save to Flash"
    Write-Host "  [4] SET_FILTER_ALPHA:0.96 - Set complementary filter alpha to 0.96"
    Write-Host "  [5] SET_FILTER_ALPHA:0.98 - Set complementary filter alpha to 0.98"
    Write-Host "  [m] Monitor Mode         - Continuous telemetry streaming"
    Write-Host "  [c] Clear screen"
    Write-Host "  [q] Quit and close port"
    Write-Host "  Or type any custom command string (e.g. 'SET_FILTER_ALPHA:0.95')"
    Write-Host "----------------------------------------------------------------------------" -ForegroundColor Gray

    $running = $true
    while ($running -and $serial.IsOpen) {
        # Check for unprompted incoming messages (e.g. streaming telemetry)
        Read-IncomingBuffer -portObj $serial

        $inputCmd = Read-Host "STM32-CLI>"
        if ([string]::IsNullOrWhiteSpace($inputCmd)) {
            continue
        }

        switch ($inputCmd.Trim()) {
            "1" { Send-Command -portObj $serial -cmd "INFO_REQUEST" }
            "2" { Send-Command -portObj $serial -cmd "GET_ANGLE" }
            "3" { Send-Command -portObj $serial -cmd "CALIBRATE_IMU" }
            "4" { Send-Command -portObj $serial -cmd "SET_FILTER_ALPHA:0.96" }
            "5" { Send-Command -portObj $serial -cmd "SET_FILTER_ALPHA:0.98" }
            "m" {
                Write-Host "[MONITOR] Streaming mode active. Press Ctrl+C or Enter to return to menu..." -ForegroundColor Cyan
                while (-not [Console]::KeyAvailable) {
                    try {
                        $line = $serial.ReadLine()
                        if ($line) {
                            $ts = (Get-Date).ToString("HH:mm:ss.fff")
                            Write-Host "[$ts] $line" -ForegroundColor Green
                        }
                    } catch [System.TimeoutException] {
                        # Heartbeat poll
                    }
                    Start-Sleep -Milliseconds 20
                }
                # Consume key
                $null = [Console]::ReadKey($true)
                Write-Host "[MONITOR] Stopped." -ForegroundColor Cyan
            }
            "c" {
                Clear-Host
            }
            "q" {
                $running = $false
            }
            default {
                # Custom command
                Send-Command -portObj $serial -cmd $inputCmd.Trim()
            }
        }
    }
} finally {
    if ($serial -and $serial.IsOpen) {
        Write-Host ""
        Write-Host "[INFO] Closing $Port..." -ForegroundColor Gray
        $serial.Close()
        $serial.Dispose()
        Write-Host "[DISCONNECTED] Port closed." -ForegroundColor Gray
    }
}
