#!/usr/bin/env bash
# =============================================================================
# Neutron Bootloader - Project Atom
# neutron  - Linux/macOS build & run orchestration
# =============================================================================

set -e

Command="${1:-build}"
Option="$2"

# ----------------------------------------------------------------
# Ensure script runs relative to project root
# ----------------------------------------------------------------
SCRIPT_PATH="$(readlink -f "$0")"
ProjectPath="$(dirname "$SCRIPT_PATH")"

cd "$ProjectPath" || exit 1

# ----------------------------------------------------------------
# Validate Neutron Project Root Structure
# ----------------------------------------------------------------
RequiredFiles=("VERSION" "build.cfg")
RequiredDirs=("neutron")
MissingItems=()

# Check required files
for file in "${RequiredFiles[@]}"; do
  if [[ ! -f "$ProjectPath/$file" ]]; then
    MissingItems+=("$file")
  fi
done

# Check required directories
for dir in "${RequiredDirs[@]}"; do
  if [[ ! -d "$ProjectPath/$dir" ]]; then
    MissingItems+=("$dir/ (directory)")
  fi
done

# Report missing items
if [[ ${#MissingItems[@]} -gt 0 ]]; then
  echo "ERROR: Invalid Neutron project root. Missing:"
  for item in "${MissingItems[@]}"; do
    echo "  - $item"
  done
  exit 1
fi

# ----------------------------------------------------------------
# Load Version from VERSION file
# ----------------------------------------------------------------
Version=$(grep '^VERSION:' VERSION | sed 's/^VERSION:[[:space:]]*//' | sed 's/^v//' | tr -d '\r')

if [[ -z "$Version" ]]; then
  echo "ERROR: VERSION entry not found in VERSION file."
  exit 1
fi

# ----------------------------------------------------------------
# Globals
# ----------------------------------------------------------------
ImageVersion="neutron-build:$Version"
ImageLatest="neutron-build:latest"
QEMU="qemu-system-aarch64"
BIN_DIR="$ProjectPath/bin"
BUILD_DIR="$ProjectPath/build"

# ----------------------------------------------------------------
# Ensure Docker Exists
# ----------------------------------------------------------------
ensure_docker() {
  if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: Docker not found in PATH."
    exit 1
  fi
}

# ----------------------------------------------------------------
# Ensure Output Directories
# ----------------------------------------------------------------
ensure_output_dir() {
  mkdir -p "$BIN_DIR" "$BUILD_DIR"
}

# ----------------------------------------------------------------
# Docker Build / Tag
# ----------------------------------------------------------------
docker_build() {
  echo "[DOCKER] Building image: $ImageVersion"
  docker build -t "$ImageVersion" .
}

docker_tag() {
  echo "[DOCKER] Tagging image as latest"
  docker tag "$ImageVersion" "$ImageLatest"
}

# ----------------------------------------------------------------
# Ensure Docker Image Exists
# ----------------------------------------------------------------
ensure_image() {
  echo "[DOCKER] Verifying image state..."
  latest_id=$(docker image inspect "$ImageLatest" --format '{{.Id}}' 2>/dev/null || true)
  version_id=$(docker image inspect "$ImageVersion" --format '{{.Id}}' 2>/dev/null || true)

  # Case 1: Version tag does not exist → must build
  if [ -z "$version_id" ]; then
    echo "[DOCKER] Version tag not found. Building image..."
    docker_build
    docker_tag

  # Case 2: Version exists but latest missing → retag
  elif [ -z "$latest_id" ]; then
    echo "[DOCKER] 'latest' tag missing. Synchronizing..."
    docker tag "$ImageVersion" "$ImageLatest"

  # Case 3: Both exist but point to different images → retag
  elif [ "$latest_id" != "$version_id" ]; then
    echo "[DOCKER] Tag mismatch detected. Synchronizing..."
    docker tag "$ImageVersion" "$ImageLatest"

  else
    echo "[DOCKER] Image verified ($ImageVersion)."
  fi

  final_id=$(docker image inspect "$ImageLatest" --format '{{.Id}}')
  echo "[DOCKER] Active image ID: $final_id"
}

# ----------------------------------------------------------------
# Run Command Inside Container
# ----------------------------------------------------------------
docker_run_cmd() {
  ensure_image
  echo "[DOCKER] Executing inside container: $1"
  echo
  docker run --rm \
    --user "$(id -u):$(id -g)" \
    --mount type=bind,src="$ProjectPath",dst=/Neutron \
    "$ImageLatest" \
    bash -c "$1"
}

# ----------------------------------------------------------------
# Interactive Shell
# ----------------------------------------------------------------
neutron_shell() {
  ensure_image
  echo "[DOCKER] Opening interactive shell..."
  docker run --rm -it \
    --user "$(id -u):$(id -g)" \
    --mount type=bind,src="$ProjectPath",dst=/Neutron \
    "$ImageLatest" \
    bash
}

# ----------------------------------------------------------------
# Docker Subcommands
# ----------------------------------------------------------------
neutron_docker() {
  case "$1" in
  build) docker_build ;;
  tag) docker_tag ;;
  bash) neutron_shell ;;
  "")
    echo
    echo "Docker Commands:"
    echo "  neutron docker build"
    echo "  neutron docker tag"
    echo "  neutron docker bash"
    echo "  neutron docker \"make clean\""
    echo
    ;;
  *)
    echo "[DOCKER] Running custom command: $1"
    docker_run_cmd "$1"
    ;;
  esac
}

# ----------------------------------------------------------------
# Build Inside Docker
# ----------------------------------------------------------------
neutron_build() {
  Target="${1:-all}"

  case "$Target" in
  all | bootloader | kernel | sd-image | clean | size) ;;
  *)
    echo "Invalid build target: '$Target'"
    echo
    echo "Valid build targets:"
    echo "  all (default)"
    echo "  bootloader"
    echo "  kernel"
    echo "  sd-image"
    echo "  clean"
    echo "  size"
    exit 1
    ;;
  esac

  echo "[BUILD] make $Target"
  docker_run_cmd "make $Target"
}

# ----------------------------------------------------------------
# Check If Build Exists
# ----------------------------------------------------------------
build_exists() {
  [[ -f "$BIN_DIR/kernel8.img" && -f "$BIN_DIR/sd.img" ]]
}

# ----------------------------------------------------------------
# Run QEMU on Host
# ----------------------------------------------------------------
neutron_run_host() {
  ForceBuild="$1"

  if ! command -v "$QEMU" >/dev/null 2>&1; then
    echo "[QEMU] Executable not found in PATH."
    exit 1
  fi

  if [[ "$ForceBuild" == "true" ]]; then
    echo "[RUN] Forced rebuild before execution..."
    docker_run_cmd "make all"
  elif ! build_exists; then
    echo "[RUN] No build artifacts found. Building..."
    docker_run_cmd "make all"
  else
    echo "[RUN] Using existing build artifacts."
  fi

  SD_IMG="$BIN_DIR/sd.img"
  BL_IMG="$BIN_DIR/kernel8.img"
  CONFIG_FILE="$ProjectPath/build.mk"

  if [ ! -f "$CONFIG_FILE" ]; then
    echo "[CONFIG] build.mk not found."
    exit 1
  fi

  EMBED_KERNEL_LINE=$(grep -E '^\s*EMBED_KERNEL\s*:=' "$CONFIG_FILE" | tr -d '\r')

  if [ -z "$EMBED_KERNEL_LINE" ]; then
    echo "[CONFIG] EMBED_KERNEL entry missing in build.mk."
    exit 1
  fi

  EMBED_KERNEL_VALUE=$(echo "$EMBED_KERNEL_LINE" | sed -E 's/^\s*EMBED_KERNEL\s*:=\s*//')

  echo "[QEMU] Launching Raspberry Pi 3 (Cortex-A53)..."

  if [ "$EMBED_KERNEL_VALUE" = "0" ]; then
    echo "[QEMU] Running with SD card (kernel loaded from SD)..."

    "$QEMU" \
      -machine raspi3b \
      -cpu cortex-a53 \
      -m 1G \
      -kernel "$BL_IMG" \
      -drive file="$SD_IMG",if=sd,format=raw \
      -serial mon:stdio \
      -display none
  else
    echo "[QEMU] Running with embedded kernel (no SD card)..."

    "$QEMU" \
      -machine raspi3b \
      -cpu cortex-a53 \
      -m 1G \
      -kernel "$BL_IMG" \
      -serial mon:stdio \
      -display none
  fi

  exit_code=$?
  if [ $exit_code -ne 0 ]; then
    echo "[QEMU] Execution failed with code $exit_code"
    exit $exit_code
  fi
}

# ----------------------------------------------------------------
# Run QEMU Inside Docker
# ----------------------------------------------------------------
neutron_run_docker() {
  ForceBuild="$1"

  ensure_image

  if [[ "$ForceBuild" == "true" ]]; then
    echo "[EMU] Forced rebuild before emulation..."
    docker_run_cmd "make all"
  elif ! build_exists; then
    echo "[EMU] No build artifacts found. Building..."
    docker_run_cmd "make all"
  else
    echo "[EMU] Using existing build artifacts."
  fi

  echo "[EMU] Launching QEMU inside container..."

  docker run --rm -it \
    --user "$(id -u):$(id -g)" \
    --mount type=bind,src="$ProjectPath",dst=/Neutron \
    "$ImageLatest" \
    bash -c "make qemu-rpi-no-build"
}

# ----------------------------------------------------------------
# Help
# ----------------------------------------------------------------
show_help() {
  echo
  echo "Neutron CLI"
  echo
  echo "Usage:"
  echo "  neutron <command> [options]"
  echo

  echo "Build Commands:"
  echo "  build [target]          Build inside Docker"
  echo "    all                   [Default] Build all artifacts"
  echo "    bootloader            Build kernel8.img only"
  echo "    kernel                Build atom.bin only"
  echo "    sd-image              Create sd.img only"
  echo "    clean                 Remove build artifacts"
  echo "    size                  Show section sizes"
  echo

  echo "Run (QEMU - Host):"
  echo "  run                     Run Neutron on QEMU (host installation)"
  echo "                          builds all artifacts if not found"
  echo "  Options:"
  echo "    --build               Force rebuild before running"
  echo

  echo "Emulation (QEMU - Docker):"
  echo "  emu                     Run Neutron on QEMU inside Docker container"
  echo "                          builds all artifacts if not found"
  echo "  Options:"
  echo "    --build               Force rebuild before running"
  echo

  echo "Shell:"
  echo "  shell                   Open interactive Docker shell"
  echo

  echo "Docker:"
  echo "  docker build            Build Docker image"
  echo "  docker tag              Tag image as latest"
  echo "  docker bash             Open interactive Docker shell"
  echo "  docker \"<command>\"    Run arbitrary bash command in container"
  echo

  echo "Help:"
  echo "  help                    Show this message"
  echo
}

# ----------------------------------------------------------------
# Main
# ----------------------------------------------------------------
ensure_docker
ensure_output_dir
echo "Neutron Bootloader - Version $Version"

case "$Command" in
build)
  neutron_build "$Option"
  ;;
run)
  [[ "$Option" == "--build" ]] && force=true || force=false
  neutron_run_host "$force"
  ;;
emu)
  [[ "$Option" == "--build" ]] && force=true || force=false
  neutron_run_docker "$force"
  ;;
docker)
  neutron_docker "$Option"
  ;;
shell)
  neutron_shell
  ;;
help)
  show_help
  ;;
*)
  echo "Unknown command: $Command"
  show_help
  exit 1
  ;;
esac
