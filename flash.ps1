
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
    Write-Host "❌ Error: arduino-cli command not found." -ForegroundColor Red
    Write-Host "Please install it or place arduino-cli.exe in the 'bin' folder."
    Write-Host "Download: https://arduino.github.io/arduino-cli/latest/installation/"
    exit 1
}

# 2. Check/Install ESP32 Core
Write-Host "Checking ESP32 core..." -ForegroundColor Yellow
$coreList = arduino-cli core list
if ($coreList -notmatch "esp32:esp32") {
    Write-Host "⚠️ ESP32 core not found. Installing..." -ForegroundColor Yellow
    arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
    arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
} else {
    Write-Host "✅ ESP32 core is installed." -ForegroundColor Green
}

# 3. Detect Port
Write-Host "Searching for connected board..." -ForegroundColor Yellow
try {
    $boardListJson = arduino-cli board list --format json | ConvertFrom-Json
} catch {
    Write-Host "⚠️ Failed to parse board list as JSON. Falling back to manual selection." -ForegroundColor Red
    $boardListJson = @()
}

$connectedBoards = @()
if ($boardListJson) {
    # arrays in json are sometimes tricky in PS if single item
    if ($boardListJson -is [System.Management.Automation.PSCustomObject]) {
        $boardListJson = @($boardListJson) 
    }
    
    foreach ($board in $boardListJson) {
        if ($board.port.address -match "COM") {
             $connectedBoards += $board
        }
    }
}

$selectedPort = $null

if ($connectedBoards.Count -eq 1) {
    $selectedPort = $connectedBoards[0].port.address
    Write-Host "✅ Auto-detected port: $selectedPort" -ForegroundColor Green
    $confirm = Read-Host "Press ENTER to use $selectedPort or type a new port"
    if (-not [string]::IsNullOrWhiteSpace($confirm)) {
        $selectedPort = $confirm
    }
} elseif ($connectedBoards.Count -gt 1) {
    Write-Host "Multiple ports found:" -ForegroundColor Yellow
    for ($i=0; $i -lt $connectedBoards.Count; $i++) {
        Write-Host " [$i] $($connectedBoards[$i].port.address) - $($connectedBoards[$i].matching_boards[0].name)"
    }
    $idx = Read-Host "Select a board (0-$($connectedBoards.Count - 1)) or type COM port"
    if ($idx -match "^\d+$" -and $idx -lt $connectedBoards.Count) {
        $selectedPort = $connectedBoards[[int]$idx].port.address
    } else {
        $selectedPort = $idx
    }
} else {
    Write-Host "⚠️ No boards auto-detected." -ForegroundColor Yellow
    $selectedPort = Read-Host "Please enter your serial port (e.g. COM3)"
}

if ([string]::IsNullOrWhiteSpace($selectedPort)) {
    Write-Host "❌ No port selected. Exiting." -ForegroundColor Red
    exit 1
}

# 4. Compile
Write-Host "Compiling sketch for $FQBN..." -ForegroundColor Cyan
arduino-cli compile --fqbn $FQBN .
if ($LASTEXITCODE -ne 0) { 
    Write-Host "❌ Compilation failed." -ForegroundColor Red
    exit $LASTEXITCODE 
}

# 5. Upload
Write-Host "Uploading to $selectedPort..." -ForegroundColor Cyan
arduino-cli upload -p $selectedPort --fqbn $FQBN .
if ($LASTEXITCODE -ne 0) { 
    Write-Host "❌ Upload failed." -ForegroundColor Red
    exit $LASTEXITCODE 
}

Write-Host "==========================================" -ForegroundColor Green
Write-Host "🎉 Done! Opening Serial Monitor..." -ForegroundColor Green
Write-Host "Press Ctrl+C to exit monitor." -ForegroundColor Gray
Write-Host "==========================================" -ForegroundColor Green

# 6. Monitor
arduino-cli monitor -p $selectedPort --config baudrate=115200
