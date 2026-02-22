
# =============================================================================
# Neutron Bootloader - Project Atom
# neutron.ps1  - docker build script for windows 
# 
# Organization : serene brew
# Author       : TriDEntApollO
# License      : BSD-3-Clause
#
# ================================================================
param (
    [string]$Command = "build",
    [string]$Option
)

# ----------------------------------------------------------------
# Ensure script runs relative to project root
# ----------------------------------------------------------------
$ProjectPath = Split-Path -Parent $MyInvocation.MyCommand.Path
# Temporarily change location to project root for all operations
Push-Location $ProjectPath

try {
    # ----------------------------------------------------------------
    # Validate Neutron Project Root Structure
    # ----------------------------------------------------------------
    $RequiredFiles = @("VERSION", "build.cfg")
    $RequiredDirs = @("neutron")
    $MissingItems = @()
    
    foreach ($file in $RequiredFiles) {
        if (-not (Test-Path (Join-Path $ProjectPath $file))) {
            $MissingItems += $file
        }
    }
    
    foreach ($dir in $RequiredDirs) {
        $FullPath = Join-Path $ProjectPath $dir
        if (-not (Test-Path $FullPath -PathType Container)) {
            $MissingItems += "$dir/ (directory)"
        }
    }
    
    if ($MissingItems.Count -gt 0) {
        Write-Error "Invalid Neutron project root. Missing:"
        foreach ($item in $MissingItems) {
            Write-Host "  - $item"
        }
        exit 1
    }
    
    # ----------------------------------------------------------------
    # Load Version from VERSION file
    # ----------------------------------------------------------------
    $VersionFile = Join-Path $ProjectPath "VERSION"
    
    $VersionLine = Get-Content $VersionFile |
    Where-Object { $_ -match '^VERSION:\s*' }
    
    if (-not $VersionLine) {
        Write-Error "VERSION entry not found in VERSION file."
        exit 1
    }
    
    # Extract value after "VERSION:"
    $Version = ($VersionLine -replace '^VERSION:\s*', '').Trim()
    # Remove leading 'v' if you want
    $Version = $Version.TrimStart('v')
    
    # ----------------------------------------------------------------
    # Globals
    # ----------------------------------------------------------------
    $ProjectPath  = (Get-Location).Path
    $ImageVersion = "neutron-build:$Version"
    $ImageLatest  = "neutron-build:latest"
    $QEMU         = "qemu-system-aarch64"
    $BIN_DIR      = Join-Path $ProjectPath "bin"
    $BUILD_DIR    = Join-Path $ProjectPath "build"
    
    # ----------------------------------------------------------------
    # Utility: Ensure Docker Exists
    # ----------------------------------------------------------------
    function Ensure-Docker {
        if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
            Write-Error "Docker not found in PATH."
            exit 1
        }
    }
    
    # ----------------------------------------------------------------
    # Utility: Ensure Output Directory Exists
    # ----------------------------------------------------------------
    function Ensure-OutputDir {
        New-Item -ItemType Directory -Path $BIN_DIR, $BUILD_DIR -Force | Out-Null
    }
    
    # ----------------------------------------------------------------
    # Docker Build / Tag
    # ----------------------------------------------------------------
    function Docker-Build {
        Write-Host "[DOCKER] Building image: $ImageVersion"
        docker build -t $ImageVersion .
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    function Docker-Tag {
        Write-Host "[DOCKER] Tagging image as latest"
        docker tag $ImageVersion $ImageLatest
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    # ----------------------------------------------------------------
    # Utility: Ensure Docker Image Exists
    # ----------------------------------------------------------------
    function Ensure-Image {
        Write-Host "[DOCKER] Verifying image state..."
        $latestId  = docker image inspect $ImageLatest  --format '{{.Id}}' 2>$null
        $versionId = docker image inspect $ImageVersion --format '{{.Id}}' 2>$null
        
        if (-not $versionId) {
            Write-Host "[DOCKER] Version tag not found. Building image..."
            Docker-Build
            Docker-Tag
        }
        elseif (-not $latestId) {
            Write-Host "[DOCKER] 'latest' tag missing. Synchronizing..."
            docker tag $ImageVersion $ImageLatest
        }
        elseif ($latestId -ne $versionId) {
            Write-Host "[DOCKER] Tag mismatch detected. Synchronizing..."
            docker tag $ImageVersion $ImageLatest
        }
        else {
            Write-Host "[DOCKER] Image verified ($ImageVersion)."
        }
        
        $finalId = docker image inspect $ImageLatest --format '{{.Id}}'
        Write-Host "[DOCKER] Active image ID: $finalId"
    }

    # ----------------------------------------------------------------
    # Run command inside container
    # ----------------------------------------------------------------
    function Docker-RunCmd($Cmd) {
        Ensure-Image
        Write-Host "[DOCKER] Executing inside container: $Cmd"
        Write-Host ""
        docker run --rm `
        --mount type=bind,src="$ProjectPath",dst=/Neutron `
        $ImageLatest `
        bash -c "$Cmd"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    # ----------------------------------------------------------------
    # Interactive Shell
    # ----------------------------------------------------------------
    function Neutron-Shell {
        Ensure-Image
        Write-Host "[DOCKER] Opening interactive shell..."
        docker run --rm -it `
        --mount type=bind,src="$ProjectPath",dst=/Neutron `
        $ImageLatest `
        bash
    }

    # ----------------------------------------------------------------
    # Run Docker commands
    # ----------------------------------------------------------------
    function Neutron-Docker($Option) {
        switch ($Option) {
            "build" {Docker-Build}
            "tag" {Docker-Tag}
            "bash" {Neutron-Shell}
            
            default {
                if ($Option) {
                    Write-Host "[DOCKER] Running custom command: $Option"
                    Docker-RunCmd $Option
                }
                else {
                    Write-Host ""
                    Write-Host "Docker Commands:"
                    Write-Host "  neutron docker build"
                    Write-Host "  neutron docker tag"
                    Write-Host "  neutron docker bash"
                    Write-Host "  neutron docker `"make clean`""
                    Write-Host ""
                }
            }
        }
    }

    # ----------------------------------------------------------------
    # Build inside Docker
    # ----------------------------------------------------------------
    function Neutron-Build($Target) {
        if (-not $Target) {
            $Target = "all"
        }
        
        switch ($Target) {
            "all"        { }
            "bootloader" { }
            "kernel"     { }
            "sd-image"   { }
            "clean"      { }
            "size"       { }
            
            default {
                Write-Error "Invalid build target: '$Target'"
                Write-Host ""
                Write-Host "Valid build targets:"
                Write-Host "  all (default)"
                Write-Host "  bootloader"
                Write-Host "  kernel"
                Write-Host "  sd-image"
                Write-Host "  clean"
                Write-Host "  size"
                exit 1
            }
        }
        
        Write-Host "[BUILD] make $Target"
        Docker-RunCmd "make $Target"
    }

    # ----------------------------------------------------------------
    # Check if build artifacts exist
    # ----------------------------------------------------------------
    function Build-Exists {
        $kernel = Join-Path $BIN_DIR "kernel8.img"
        $sdimg  = Join-Path $BIN_DIR "sd.img"
        return (Test-Path $kernel) -and (Test-Path $sdimg)
    }

    # ----------------------------------------------------------------
    # Run QEMU on Host
    # ----------------------------------------------------------------
    function Neutron-RunHost($ForceBuild) {
        if (-not (Get-Command $QEMU -ErrorAction SilentlyContinue)) {
            Write-Error "QEMU not found in PATH."
            exit 1
        }
        
        if ($ForceBuild) {
            Write-Host "[RUN] Forced rebuild before execution..."
            Docker-RunCmd "make all"
        } elseif (-not (Build-Exists)) {
            Write-Host "[RUN] No build artifacts found. Building..."
            Docker-RunCmd "make all"
        } else {
            Write-Host "[RUN] Using existing build artifacts."
        }
        
        $SD_IMG = Join-Path $BIN_DIR "sd.img"
        $BL_IMG = Join-Path $BIN_DIR "kernel8.img"
        $ConfigFile = Join-Path (Get-Location) "build.mk"
        
        if (-not (Test-Path $ConfigFile)) {
            Write-Error "[CONFIG] build.mk not found."
            exit 1
        }
        
        $EmbedKernelLine = Get-Content $ConfigFile |
        Where-Object { $_ -match '^\s*EMBED_KERNEL\s*:=' }
        
        if (-not $EmbedKernelLine) {
            Write-Error "[CONFIG] EMBED_KERNEL entry missing in build.mk."
            exit 1
        }
        
        $EmbedKernelValue = ($EmbedKernelLine -replace '^\s*EMBED_KERNEL\s*:=\s*', '').Trim()
        
        Write-Host "[QEMU] Launching Raspberry Pi 3 (Cortex-A53)..."
        
        if  ($EmbedKernelValue -eq "0") {
            Write-Host "[QEMU] Running with SD card (kernel loaded from SD)..."
            & $QEMU `
            -machine raspi3b `
            -cpu cortex-a53 `
            -m 1G `
            -kernel $BL_IMG `
            -drive file=$SD_IMG,if=sd,format=raw `
            -serial mon:stdio `
            -display none
        } else {
            Write-Host "[QEMU] Running with embedded kernel (no SD card)..."
            & $QEMU `
            -machine raspi3b `
            -cpu cortex-a53 `
            -m 1G `
            -kernel $BL_IMG `
            -serial mon:stdio `
            -display none
        }
        
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    # ----------------------------------------------------------------
    # Run QEMU inside Docker
    # ----------------------------------------------------------------
    function Neutron-RunDocker($ForceBuild) {
        Ensure-Image
        
        if ($ForceBuild) {
            Write-Host "[EMU] Forced rebuild before emulation..."
            Docker-RunCmd "make all"
        } elseif (-not (Build-Exists)) {
            Write-Host "[EMU] No build artifacts found. Building..."
            Docker-RunCmd "make all"
        } else {
            Write-Host "[EMU] Using existing build artifacts."
        }
        
        Write-Host "[EMU] Launching QEMU inside container..."
        
        docker run --rm -it `
        --mount type=bind,src="$ProjectPath",dst=/Neutron `
        $ImageLatest `
        bash -c "make qemu-rpi-no-build"
        
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    # ----------------------------------------------------------------
    # Help Mesage
    # ----------------------------------------------------------------
    function Show-Help {
        
        Write-Host ""
        Write-Host "Neutron CLI"
        Write-Host ""
        Write-Host "Usage:"
        Write-Host "  neutron <command> [options]"
        Write-Host ""
        
        Write-Host "Build Commands:"
        Write-Host "  build               Build inside Docker"
        Write-Host "    all               [Default] Build all artifacts"
        Write-Host "    bootloader        Build kernel8.img only"
        Write-Host "    kernel            Build atom.bin only"
        Write-Host "    sd-image          Create sd.img only"
        Write-Host "    clean             Remove build artifacts"
        Write-Host "    size              Show section sizes"
        Write-Host ""
        
        Write-Host "Run (QEMU - Host):"
        Write-Host "  run                 Run Neutron on QEMU (host installation)"
        Write-Host "                      builds all artifacts if not found"
        Write-Host "  Options:"
        Write-Host "    --build           Force rebuild before running"
        Write-Host ""
        
        Write-Host "Emulation (QEMU - Docker):"
        Write-Host "  emu                 Run Neutron on QEMU inside Docker container"
        Write-Host "                      builds all artifacts if not found"
        Write-Host "  Options:"
        Write-Host "    --build           Force rebuild before running"
        Write-Host ""
        
        Write-Host "Shell:"
        Write-Host "  shell               Open interactive Docker shell"
        Write-Host ""
        
        Write-Host "Docker:"
        Write-Host "  docker build        Build Docker image"
        Write-Host "  docker tag          Tag image as latest"
        Write-Host "  docker bash         Open interactive Docker shell"
        Write-Host "  docker <command>    Run arbitrary bash command in container"
        Write-Host ""
        
        Write-Host "Help:"
        Write-Host "  help                Show this message"
        Write-Host ""
    }

    # ----------------------------------------------------------------
    # Command Routing
    # ----------------------------------------------------------------
    Ensure-Docker
    Ensure-OutputDir
    Write-Host "Neutron Bootloader - Version $Version"

    switch ($Command) {
        
        # ============================================================
        # BUILD
        # ============================================================
        "build" {
            Neutron-Build $Option
        }
        
        # ============================================================
        # RUN (Host)
        # ============================================================
        "run" {
            $force = ($Option -eq "--build")
            Neutron-RunHost $force
        }
        
        # ============================================================
        # EMU (Docker QEMU)
        # ============================================================
        "emu" {
            $force = ($Option -eq "--build")
            Neutron-RunDocker $force
        }
        
        # ============================================================
        # DOCKER
        # ============================================================
        "docker" {
            Neutron-Docker $Option
        }
        
        # ============================================================
        # SHELL
        # ============================================================
        "shell" {
            Neutron-Shell
        }
        
        # ============================================================
        # HELP
        # ===========================================================
        "help" {
            Show-Help
        }
        
        default {
            Write-Host "Unknown command: $Command"
            Show-Help
            exit 1
        }
    }

}
finally {
    # Restore original location
    Pop-Location
}
