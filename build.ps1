param (
    [ValidateSet("all", "bootloader", "kernel", "sd-image", "clean", "size")]
    [string]$Target = "all"
)

# Ensure Docker exists
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Error "Docker is not installed or not in PATH."
    exit 1
}

# Resolve absolute Windows path safely
$ProjectPath = (Get-Location).Path

Write-Host "----------------------------------------"
Write-Host " Neutron Docker Build"
Write-Host " Target  : $Target"
Write-Host " Path    : $ProjectPath"
Write-Host "----------------------------------------"

docker run --rm `
    --mount type=bind,src="$ProjectPath",dst=/Neutron `
    neutron-build `
    bash -c "make $Target"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
    exit $LASTEXITCODE
}

Write-Host "Build completed successfully."
