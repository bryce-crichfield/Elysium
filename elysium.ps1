param()

# Get the root path
$ELYSIUM_ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path

# Function definitions
function Elysium_Clean {
    Write-Host "Cleaning build artifacts..."
    if (Test-Path "$ELYSIUM_ROOT\Build") {
        Remove-Item -Recurse -Force "$ELYSIUM_ROOT\Build"
    }
    if (Test-Path "$ELYSIUM_ROOT\Binary") {
        Remove-Item -Recurse -Force "$ELYSIUM_ROOT\Binary"
    }
    Write-Host "Clean complete"
}

function Elysium_Build {
    Write-Host "Building Elysium..."
    $currentLocation = Get-Location

    try {
        # Create necessary directories
        New-Item -ItemType Directory -Force -Path "$ELYSIUM_ROOT\Build" | Out-Null
        New-Item -ItemType Directory -Force -Path "$ELYSIUM_ROOT\Binary" | Out-Null

        Set-Location "$ELYSIUM_ROOT\Build"

        # Use Ninja generator with MinGW and override shell
        $env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
        $env:ComSpec = "C:\Windows\System32\cmd.exe"
        cmake -G "Ninja" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_MAKE_PROGRAM=ninja ..
        if ($LASTEXITCODE -eq 0) {
            cmake --build . --config Release
        }
    }
    finally {
        Set-Location $currentLocation
    }
}

function Elysium_Run {
    $currentLocation = Get-Location

    try {
        Set-Location "$ELYSIUM_ROOT\Binary"
        Start-Process -FilePath ".\Elysium.exe" -NoNewWindow
    }
    finally {
        Set-Location $currentLocation
    }
}

# Parse command-line arguments
$operations = @()
$shouldClean = $false
$shouldBuild = $false
$shouldRun = $false

foreach ($arg in $args) {
    switch -Regex ($arg) {
        "^--[Cc]lean$" { $shouldClean = $true }
        "^--[Bb]uild$" { $shouldBuild = $true }
        "^--[Rr]un$" { $shouldRun = $true }
        "^--[Hh]elp$" {
            Write-Host "Usage: .\script.ps1 [--Clean] [--Build] [--Run] [--Help]"
            Write-Host "  --Clean: Clean build artifacts"
            Write-Host "  --Build: Build the project"
            Write-Host "  --Run: Run the executable"
            Write-Host "  --Help: Show this help message"
            Write-Host ""
            Write-Host "Operations are executed in order: Clean -> Build -> Run"
            exit 0
        }
        default {
            Write-Warning "Unknown argument: $arg"
            Write-Host "Use --Help for usage information"
        }
    }
}

# Execute operations in the correct order
if ($shouldClean) {
    Elysium_Clean
}

if ($shouldBuild) {
    Elysium_Build
}

if ($shouldRun) {
    Elysium_Run
}

# If no arguments provided, show help
if ($args.Count -eq 0) {
    Write-Host "No arguments provided. Use --Help for usage information."
    Write-Host "Available options: --Clean, --Build, --Run"
}
