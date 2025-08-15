param()

# Get the root path
$ELYSIUM_ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path

function Elysium_Setup {
    Write-Host "Setting up Elysium environment..."
    $msys64Path = "C:\msys64"
    $msys2DownloadUrl = "https://github.com/msys2/msys2-installer/releases/download/2024-01-13/msys2-x86_64-20240113.exe"
    $msys2InstallerPath = "msys2-installer.exe"

    if (-not (Test-Path -Path $msys64Path)) {
        if (-not (Test-Path -Path $msys2InstallerPath)) {
            Write-Host "Downloading MSYS2..."
            $ProgressPreference = 'SilentlyContinue'
            Invoke-WebRequest `
                -Uri $msys2DownloadUrl `
                -OutFile $msys2InstallerPath
            $ProgressPreference = 'Continue'
            Write-Host "MSYS2 Downloaded"
        }

        # Install MSYS2 silently
        Write-Host "Installing MSYS2..."
        Start-Process -FilePath $msys2InstallerPath -ArgumentList @(
            "install",
            "--confirm-command",
            "--accept-messages",
            "--root", $msys64Path
        ) -Wait -NoNewWindow
        Remove-Item -Path $msys2InstallerPath -Force
        Write-Host "MSYS2 Installation completed"
    } else {
        Write-Host "MSYS2 already installed at $msys64Path"
    }

    # Update MSYS2 and install necessary packages
    Write-Host "Updating MSYS2 and installing required packages..."
    
    # First update the package database and core system
    & "$msys64Path\usr\bin\bash.exe" -lc "pacman -Syu --noconfirm"
    
    # Install development packages
    & "$msys64Path\usr\bin\bash.exe" -lc "pacman -S --noconfirm base-devel mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja"
    
    # Clean package cache
    & "$msys64Path\usr\bin\bash.exe" -lc "pacman -Scc --noconfirm"
    
    Write-Host "Elysium environment setup completed!"
}
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
$shouldSetup = $false
$shouldClean = $false
$shouldBuild = $false
$shouldRun = $false

foreach ($arg in $args) {
    switch -Regex ($arg) {
        "^--[Ss]etup$" { $shouldSetup = $true }
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
if ($shouldSetup) {
    Elysium_Setup
}

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
