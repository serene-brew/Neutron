
# =============================================================================
# Neutron Bootloader - Project Atom
# build.ps1  - docker build script for windows 
# 
# Organization : serene brew
# Author       : mintRaven-05
# License      : BSD-3-Clause
#
# ================================================================

param (
    [Parameter(Mandatory=$true)]
    [string]$Command
)

$ImageVersion = "neutron-build:0.1.0"
$ImageLatest  = "neutron-build:latest"
$ProjectPath  = (Get-Location).Path

function Ensure-Docker {
    if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
        Write-Error "Docker is not installed or not in PATH."
        exit 1
    }
}

function Docker-Build {
    Write-Host "Building Docker image: $ImageVersion"
    docker build -t $ImageVersion .
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Docker-Tag {
    Write-Host "Tagging image:"
    Write-Host "  $ImageVersion -> $ImageLatest"
    docker tag $ImageVersion $ImageLatest
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Run-Make($Target) {
    Write-Host "Running make $Target inside container..."
    docker run --rm `
        --mount type=bind,src="$ProjectPath",dst=/Neutron `
        $ImageLatest `
        bash -c "make $Target"

    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Ensure-Docker

switch ($Command) {
    "docker-build" { Docker-Build }
    "tag"          { Docker-Tag }
    "all"          { Run-Make "all" }
    "bootloader"   { Run-Make "bootloader" }
    "kernel"       { Run-Make "kernel" }
    "sd-image"     { Run-Make "sd-image" }
    "clean"        { Run-Make "clean" }
    "size"         { Run-Make "size" }
    default {
        Write-Host ""
        Write-Host "Usage:"
        Write-Host "  .\build.ps1 docker-build"
        Write-Host "  .\build.ps1 tag"
        Write-Host "  .\build.ps1 all | bootloader | kernel | sd-image | clean | size"
        exit 1
    }
}
