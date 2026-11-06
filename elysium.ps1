param()

# Get the root path
$ELYSIUM_ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path

function Elysium_Setup {
    Write-Host "Setting up Elysium environment..."

    git submodule update --init --recursive

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
    & "$msys64Path\usr\bin\bash.exe" -lc "pacman -S --noconfirm base-devel mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gdb"
    
    # Clean package cache
    & "$msys64Path\usr\bin\bash.exe" -lc "pacman -Scc --noconfirm"
    
    Write-Host "Elysium environment setup completed!"
}
# Emscripten, the compiler for the Web build, installed into Tools/emsdk at a pinned version.
$EMSDK_ROOT = "$ELYSIUM_ROOT\Tools\emsdk"
$EMSDK_VERSION = "4.0.10"

function Elysium_SetupWeb {
    Write-Host "Setting up Emscripten $EMSDK_VERSION..."
    if (-not (Test-Path -Path $EMSDK_ROOT)) {
        git clone https://github.com/emscripten-core/emsdk.git $EMSDK_ROOT
    }
    & "$EMSDK_ROOT\emsdk.bat" install $EMSDK_VERSION
    if ($LASTEXITCODE -ne 0) { throw "emsdk install failed" }
    & "$EMSDK_ROOT\emsdk.bat" activate $EMSDK_VERSION
    if ($LASTEXITCODE -ne 0) { throw "emsdk activate failed" }
    Write-Host "Emscripten setup completed!"
}

# Puts emcc, emcmake and emrun (and the node and python they need) on this session's PATH.
function Use-Emsdk {
    if (-not (Test-Path -Path "$EMSDK_ROOT\emsdk_env.ps1")) {
        throw "Emscripten isn't installed. Run: .\elysium.ps1 --Setup --Platform=Web"
    }
    . "$EMSDK_ROOT\emsdk_env.ps1" | Out-Null
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
    param([bool]$Tracy, [string]$Mode, [string]$Target)
    # Release is CMake's RelWithDebInfo: -O2, with symbols kept so Tracy and gdb still read it.
    $buildType = if ($Mode -eq "Release") { "RelWithDebInfo" } else { "Debug" }
    Write-Host "Building Elysium ($Mode)..."
    $currentLocation = Get-Location

    try {
        # Create necessary directories
        New-Item -ItemType Directory -Force -Path "$ELYSIUM_ROOT\Build" | Out-Null
        New-Item -ItemType Directory -Force -Path "$ELYSIUM_ROOT\Binary" | Out-Null

        Set-Location "$ELYSIUM_ROOT"

        # Use Ninja generator with MinGW and override shell
        $env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
        $env:ComSpec = "C:\Windows\System32\cmd.exe"
        # --Tracy turns the profiler on. Its worker threads sleep through winpthreads, and the
        # statically linked winpthreads in this mingw-w64/GCC build corrupts the stack canary in
        # nanosleep ("stack smashing detected" on the "Tracy DXT1" thread). So a Tracy build
        # links winpthreads dynamically and ships libwinpthread-1.dll beside the exe.
        $tracyOption = if ($Tracy) { "ON" } else { "OFF" }
        $linkStatic = if ($Tracy) { "-static-libgcc -static-libstdc++" } else { "-static -static-libgcc -static-libstdc++" }
        cmake -G "Ninja" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_MAKE_PROGRAM=ninja "-DCMAKE_BUILD_TYPE=$buildType" "-DTRACY_ENABLE=$tracyOption" "-DCMAKE_EXE_LINKER_FLAGS=$linkStatic" -B Build
        if ($LASTEXITCODE -eq 0) {
            # --Target builds one layer (ElysiumCore, ElysiumServices, ElysiumEditor) or the Elysium exe.
            if ($Target) { cmake --build Build --target $Target } else { cmake --build Build }
            if ($Tracy) {
                Copy-Item "C:\msys64\mingw64\bin\libwinpthread-1.dll" "$ELYSIUM_ROOT\Binary" -Force
            }
        }
    }
    finally {
        Set-Location $currentLocation
    }
}

# The Web build: Build/Web and Binary/Web (Elysium.html/.js/.wasm/.data). The project is packed
# into the page at build time, so it's chosen here rather than at --Run.
function Elysium_BuildWeb {
    param([string]$Mode, [string]$Target, [string]$ProjectPath)
    $buildType = if ($Mode -eq "Release") { "Release" } else { "Debug" }
    Write-Host "Building Elysium for Web ($Mode)..."
    $currentLocation = Get-Location

    try {
        Set-Location "$ELYSIUM_ROOT"
        Use-Emsdk
        $env:PATH = "$env:PATH;C:\msys64\mingw64\bin"  # ninja; emsdk's own tools stay first

        $configure = @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=ninja", "-DCMAKE_BUILD_TYPE=$buildType", "-B", "Build/Web")
        if ($ProjectPath) {
            $webProject = $ProjectPath.TrimEnd('\', '/').Replace('\', '/')
            $configure += "-DELYSIUM_WEB_PROJECT=$webProject"
        }
        emcmake cmake @configure
        if ($LASTEXITCODE -eq 0) {
            if ($Target) { cmake --build Build/Web --target $Target } else { cmake --build Build/Web }
        }
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    finally {
        Set-Location $currentLocation
    }
}

function Elysium_Test {
    & "$ELYSIUM_ROOT\Binary\ElysiumTests.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Elysium_Run {
    param(
        [string]$ProjectPath,
        [bool]$EditorMode
    )

    $currentLocation = Get-Location

    try {
        Set-Location "$ELYSIUM_ROOT\Binary"

        if ([string]::IsNullOrEmpty($ProjectPath)) {
            throw "Project path is required. Specify it with --Project=<path>."
        }

        $ProjectPath = '{0}\Project.xml' -f $ProjectPath.TrimEnd('\', '/')

        if ([System.IO.Path]::IsPathRooted($ProjectPath)) {
            $resolvedProject = $ProjectPath
        } else {
            $resolvedProject = "$ELYSIUM_ROOT\$ProjectPath"
        }

        if (-not (Test-Path -Path $resolvedProject -PathType Leaf)) {
            throw "Project file not found: $resolvedProject"
        }

        $exeArgs = @("--Project=$resolvedProject")
        if ($EditorMode) {
            $exeArgs += "--Editor=true"
        }

        Start-Process -FilePath ".\Elysium.exe" -ArgumentList $exeArgs -NoNewWindow
    }
    finally {
        Set-Location $currentLocation
    }
}

# Serves Binary/Web on localhost and opens the page in the default browser.
function Elysium_RunWeb {
    if (-not (Test-Path -Path "$ELYSIUM_ROOT\Binary\Web\Elysium.html")) {
        throw "No Web build. Run: .\elysium.ps1 --Build --Platform=Web"
    }
    Use-Emsdk
    emrun --no_emrun_detect "$ELYSIUM_ROOT\Binary\Web\Elysium.html"
}

# Parse command-line arguments
$shouldSetup = $false
$shouldClean = $false
$shouldBuild = $false
$shouldRun = $false
$shouldTest = $false
$tracyArg = $false
$modeArg = "Debug"
$targetArg = $null
$projectArg = $null
$editorArg = $false
$platformArg = "Desktop"

foreach ($arg in $args) {
    switch -Regex ($arg) {
        "^--[Ss]etup$" { $shouldSetup = $true }
        "^--[Cc]lean$" { $shouldClean = $true }
        "^--[Bb]uild$" { $shouldBuild = $true }
        "^--[Rr]un$" { $shouldRun = $true }
        "^--[Tt]est$" { $shouldTest = $true }
        "^--[Tt]racy$" { $tracyArg = $true }
        "^--[Mm]ode=([Dd]ebug|[Rr]elease)$" { $modeArg = (Get-Culture).TextInfo.ToTitleCase($matches[1].ToLower()) }
        "^--[Tt]arget=(.+)$" { $targetArg = $matches[1] }
        "^--[Pp]roject=(.+)$" { $projectArg = $matches[1] }
        "^--[Pp]latform=([Dd]esktop|[Ww]eb)$" { $platformArg = (Get-Culture).TextInfo.ToTitleCase($matches[1].ToLower()) }
        "^--[Ee]ditor$" { $editorArg = $true }
        "^--[Ee]ditor=(.+)$" { $editorArg = ($matches[1] -eq "true") }
        "^--[Hh]elp$" {
            Write-Host "Usage: .\script.ps1 [--Clean] [--Build] [--Tracy] [--Mode=Debug|Release] [--Target=<name>] [--Test] [--Run] [--Project=<path>] [--Editor] [--Platform=Desktop|Web] [--Help]"
            Write-Host "  --Clean: Clean build artifacts"
            Write-Host "  --Build: Build the project"
            Write-Host "  --Tracy: Build with the Tracy profiler (v0.12.2) enabled"
            Write-Host "  --Mode=Debug|Release: Build type (default Debug); Release is optimized (-O2) with symbols"
            Write-Host "  --Target=<name>: Build only this target: ElysiumCore, ElysiumServices, ElysiumEditor or Elysium (default: everything)"
            Write-Host "  --Test: Run the unit tests (Binary\ElysiumTests.exe); exits non-zero on failure"
            Write-Host "  --Run: Run the executable"
            Write-Host "  --Project=<path>: Project directory to load, e.g. Projects\DemoGame (required for --Run)"
            Write-Host "  --Editor: Start in editor mode"
            Write-Host "  --Platform=Desktop|Web: What --Setup, --Build and --Run work on (default Desktop)."
            Write-Host "      Web: --Setup installs Emscripten, --Build makes Binary\Web\Elysium.html with --Project"
            Write-Host "      packed in (default Projects\Battler), --Run serves it and opens the browser"
            Write-Host "  --Help: Show this help message"
            Write-Host ""
            Write-Host "Operations are executed in order: Clean -> Build -> Test -> Run"
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
    if ($platformArg -eq "Web") { Elysium_SetupWeb } else { Elysium_Setup }
}

if ($shouldClean) {
    Elysium_Clean
}

if ($shouldBuild) {
    if ($platformArg -eq "Web") {
        Elysium_BuildWeb -Mode $modeArg -Target $targetArg -ProjectPath $projectArg
    } else {
        Elysium_Build -Tracy $tracyArg -Mode $modeArg -Target $targetArg
    }
}

if ($shouldTest) {
    Elysium_Test
}

if ($shouldRun) {
    if ($platformArg -eq "Web") { Elysium_RunWeb } else { Elysium_Run -ProjectPath $projectArg -EditorMode $editorArg }
}

# If no arguments provided, show help
if ($args.Count -eq 0) {
    Write-Host "No arguments provided. Use --Help for usage information."
    Write-Host "Available options: --Clean, --Build, --Test, --Run"
}
