$ELYSIUM_ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path

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
        cmake .. 
        if ($LASTEXITCODE -eq 0) {
            make
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