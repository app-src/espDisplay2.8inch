
# Configuration
$FQBN = "esp32:esp32:esp32s3"
$ScriptPath = $PSScriptRoot
$BinPath = Join-Path $ScriptPath "bin"

# Function to check for executable
function Test-Command ($command) {
    if (Get-Command $command -ErrorAction SilentlyContinue) {
        return $true
    }
    return $false
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "    ESP32 Build & Flash Script (Windows)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Check for arduino-cli
if (-not (Test-Command "arduino-cli")) {
    Write-Host "Error: arduino-cli command not found." -ForegroundColor Red
    Write-Host "Download: https://arduino.github.io/arduino-cli/latest/installation/"
    exit 1
}

# 2. Check/Install ESP32 Core
Write-Host "Checking ESP32 core..." -ForegroundColor Yellow
$coreList = arduino-cli core list --format text 2>&1
if ($coreList -notmatch "esp32:esp32") {
    Write-Host "ESP32 core not found. Installing..." -ForegroundColor Yellow
    arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
    arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
} else {
    Write-Host "ESP32 core is installed." -ForegroundColor Green
}

# 3. Detect Port - using detected_ports JSON structure
Write-Host "Searching for connected board..." -ForegroundColor Yellow

$selectedPort = $null

try {
    $rawJson = arduino-cli board list --format json 2>$null
    $parsed = $rawJson | ConvertFrom-Json
    
    # arduino-cli returns { detected_ports: [ { port: { address: "COM8", ... }, matching_boards: [...] } ] }
    $ports = @()
    if ($parsed.detected_ports) {
        foreach ($entry in $parsed.detected_ports) {
            if ($entry.port.address -match "^COM\d+$") {
                $portInfo = @{
                    Address = $entry.port.address
                    Label   = $entry.port.protocol_label
                    Name    = if ($entry.matching_boards -and $entry.matching_boards.Count -gt 0) { $entry.matching_boards[0].name } else { "Unknown" }
                }
                $ports += $portInfo
            }
        }
    }

    if ($ports.Count -eq 1) {
        # Auto-select the only available port
        $selectedPort = $ports[0].Address
        Write-Host "Auto-detected port: $selectedPort ($($ports[0].Name))" -ForegroundColor Green
    } elseif ($ports.Count -gt 1) {
        # Show options
        Write-Host "Multiple ports found:" -ForegroundColor Yellow
        for ($i = 0; $i -lt $ports.Count; $i++) {
            Write-Host " [$i] $($ports[$i].Address) - $($ports[$i].Name) ($($ports[$i].Label))"
        }
        $idx = Read-Host "Select a port (0-$($ports.Count - 1)) or type a COM port"
        if ($idx -match "^\d+$" -and [int]$idx -lt $ports.Count) {
            $selectedPort = $ports[[int]$idx].Address
        } else {
            $selectedPort = $idx
        }
    } else {
        Write-Host "No boards auto-detected." -ForegroundColor Yellow
        # Fallback: list all COM ports from system
        $systemPorts = @([System.IO.Ports.SerialPort]::GetPortNames())
        if ($systemPorts.Count -eq 1) {
            $selectedPort = $systemPorts[0]
            Write-Host "Found system COM port: $selectedPort" -ForegroundColor Green
        } elseif ($systemPorts.Count -gt 1) {
            Write-Host "Available system COM ports:" -ForegroundColor Yellow
            for ($i = 0; $i -lt $systemPorts.Count; $i++) {
                Write-Host " [$i] $($systemPorts[$i])"
            }
            $idx = Read-Host "Select a port (0-$($systemPorts.Count - 1)) or type a COM port"
            if ($idx -match "^\d+$" -and [int]$idx -lt $systemPorts.Count) {
                $selectedPort = $systemPorts[[int]$idx]
            } else {
                $selectedPort = $idx
            }
        } else {
            $selectedPort = Read-Host "No COM ports found. Enter port manually (e.g. COM3)"
        }
    }
} catch {
    Write-Host "Failed to detect ports: $_" -ForegroundColor Red
    $selectedPort = Read-Host "Enter COM port manually (e.g. COM3)"
}

if ([string]::IsNullOrWhiteSpace($selectedPort)) {
    Write-Host "No port selected. Exiting." -ForegroundColor Red
    exit 1
}

Write-Host "Using port: $selectedPort" -ForegroundColor Cyan

# 4. Compile
Write-Host "Compiling sketch for $FQBN..." -ForegroundColor Cyan
arduino-cli compile --fqbn $FQBN .
if ($LASTEXITCODE -ne 0) { 
    Write-Host "Compilation failed." -ForegroundColor Red
    exit $LASTEXITCODE 
}

# 5. Upload
Write-Host "Uploading to $selectedPort..." -ForegroundColor Cyan
arduino-cli upload -p $selectedPort --fqbn $FQBN .
if ($LASTEXITCODE -ne 0) { 
    Write-Host "Upload failed." -ForegroundColor Red
    exit $LASTEXITCODE 
}

Write-Host "==========================================" -ForegroundColor Green
Write-Host "Done! Opening Serial Monitor..." -ForegroundColor Green
Write-Host "Press Ctrl+C to exit monitor." -ForegroundColor Gray
Write-Host "==========================================" -ForegroundColor Green

# 6. Monitor
arduino-cli monitor -p $selectedPort --config baudrate=115200
