#!/usr/bin/env bash
# SpockD3D9 - macOS native D3D9 packaging script
# Builds for the host architecture (arm64 or x86_64) by default.
# Use --arch universal for an arm64 + x86_64 fat binary via lipo.

set -e

if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: $0 version destdir [--no-package] [--dev-build] [--arch arm64|x86_64|universal] [--build-id]"
  exit 1
fi

DXVK_VERSION="$1"
DXVK_SRC_DIR=$(dirname "$(realpath "$0")")
DXVK_OUT_DIR="$2"
mkdir -p "$DXVK_OUT_DIR"
DXVK_OUT_DIR=$(cd "$DXVK_OUT_DIR" && pwd -P)
DXVK_BUILD_DIR="$DXVK_OUT_DIR/spockd3d9-$DXVK_VERSION"
DXVK_ARCHIVE_PATH="$DXVK_OUT_DIR/spockd3d9-$DXVK_VERSION.tar.gz"

if [ -e "$DXVK_BUILD_DIR" ]; then
  echo "Build directory $DXVK_BUILD_DIR already exists"
  exit 1
fi

shift 2

opt_nopackage=0
opt_devbuild=0
opt_buildid=false
opt_arch=""

CC=${CC:="cc"}
CXX=${CXX:="c++"}

while [ $# -gt 0 ]; do
  case "$1" in
  "--no-package")
    opt_nopackage=1
    ;;
  "--dev-build")
    opt_nopackage=1
    opt_devbuild=1
    ;;
  "--build-id")
    opt_buildid=true
    ;;
  "--arch")
    shift
    opt_arch="$1"
    ;;
  *)
    echo "Unrecognized option: $1" >&2
    exit 1
  esac
  shift
done

# Detect architecture if not specified
if [ -z "$opt_arch" ]; then
  opt_arch=$(uname -m)
fi

echo "Building SpockD3D9 $DXVK_VERSION for macOS $opt_arch"

# Build a single-architecture slice and install it into $2 (prefix root).
# $1 = target arch (arm64 or x86_64)
# $2 = install prefix (e.g. $DXVK_BUILD_DIR/usr)
function build_arch_to {
  local arch="$1"
  local prefix="$2"
  local builddir="$DXVK_BUILD_DIR/build.$arch"

  cd "$DXVK_SRC_DIR"

  local strip_flag=
  if [ $opt_devbuild -eq 0 ]; then
    strip_flag=--strip
  fi

  CC="$CC" CXX="$CXX" CFLAGS="-arch $arch" CXXFLAGS="-arch $arch" \
    meson setup \
        --buildtype "release"                \
        --prefix "$prefix"                   \
        $strip_flag                          \
        --bindir "lib"                       \
        --libdir "lib"                       \
        -Dbuild_id=$opt_buildid              \
        -Denable_d3d9=true                   \
        -Denable_d3d8=false                  \
        -Denable_d3d10=false                 \
        -Denable_d3d11=false                 \
        -Denable_dxgi=false                  \
        --force-fallback-for=libdisplay-info \
        "$builddir"

  cd "$builddir"
  ninja install

  if [ $opt_devbuild -eq 0 ]; then
    rm -rf "$builddir"
  fi
}

# Convenience wrapper that installs into the default $DXVK_BUILD_DIR/usr prefix.
function build_arch {
  build_arch_to "$1" "$DXVK_BUILD_DIR/usr"
}

# Merge two per-arch staging trees into a universal (fat) binary tree using lipo.
# $1 = arch A (e.g. arm64)
# $2 = arch B (e.g. x86_64)
# $3 = output prefix (e.g. $DXVK_BUILD_DIR/usr)
function lipo_merge {
  local src_a="$DXVK_BUILD_DIR/usr.$1"
  local src_b="$DXVK_BUILD_DIR/usr.$2"
  local out="$3"

  mkdir -p "$out"

  # Copy non-binary files from arch A (headers, pkg-config, etc.)
  rsync -a --exclude='*.dylib' --exclude='*.a' "$src_a/" "$out/"

  # lipo-merge every dylib and static lib.
  find "$src_a" \( -name '*.dylib' -o -name '*.a' \) | while read -r lib_a; do
    local rel="${lib_a#$src_a/}"
    local lib_b="$src_b/$rel"
    local dest="$out/$rel"
    mkdir -p "$(dirname "$dest")"

    if [ -f "$lib_b" ]; then
      lipo -create "$lib_a" "$lib_b" -output "$dest"
    else
      echo "Warning: $lib_b not found; copying $1 slice only" >&2
      cp "$lib_a" "$dest"
    fi
  done
}

function package {
  cd "$DXVK_BUILD_DIR"
  tar -czf "$DXVK_ARCHIVE_PATH" "usr"
  cd ".."
  rm -R "spockd3d9-$DXVK_VERSION"
}

if [ "$opt_arch" = "universal" ]; then
  mkdir -p "$DXVK_BUILD_DIR"

  echo "Building arm64 slice..."
  build_arch_to "arm64"  "$DXVK_BUILD_DIR/usr.arm64"

  echo "Building x86_64 slice..."
  build_arch_to "x86_64" "$DXVK_BUILD_DIR/usr.x86_64"

  echo "Merging slices into universal binary..."
  lipo_merge "arm64" "x86_64" "$DXVK_BUILD_DIR/usr"

  rm -rf "$DXVK_BUILD_DIR/usr.arm64" "$DXVK_BUILD_DIR/usr.x86_64"
else
  build_arch "$opt_arch"
fi

if [ $opt_nopackage -eq 0 ]; then
  package
fi

echo "Done. Output: $DXVK_BUILD_DIR"
