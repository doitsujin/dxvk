#!/usr/bin/env bash

set -e

shopt -s extglob

if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: $0 version destdir [--no-package] [--dev-build]"
  exit 1
fi

DXVK_VERSION="$1"
DXVK_SRC_DIR=$(readlink -f "$0")
DXVK_SRC_DIR=$(dirname "$DXVK_SRC_DIR")
DXVK_BUILD_DIR=$(realpath "$2")"/dxvk-$DXVK_VERSION"
DXVK_ARCHIVE_PATH=$(realpath "$2")"/dxvk-$DXVK_VERSION.tar.gz"

if [ -e "$DXVK_BUILD_DIR" ]; then
  echo "Build directory $DXVK_BUILD_DIR already exists"
  exit 1
fi

shift 2

opt_nopackage=0
opt_devbuild=0
opt_buildid=false
opt_64_only=0
opt_32_only=0
opt_arm64x_only=0
opt_build_arm64x=0

crossfile="build-"

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
  "--64-only")
    opt_64_only=1
    ;;
  "--32-only")
    opt_32_only=1
    ;;
  "--arm64x-only")
    opt_arm64x_only=1
    ;;
  "--build-arm64x")
    opt_build_arm64x=1
    ;;
  *)
    echo "Unrecognized option: $1" >&2
    exit 1
  esac
  shift
done

function build_arch {
  export WINEARCH="win$1"
  export WINEPREFIX="$DXVK_BUILD_DIR/wine.$1"
  
  cd "$DXVK_SRC_DIR"

  opt_strip=
  if [ $opt_devbuild -eq 0 ]; then
    opt_strip=--strip
  fi

  meson setup --cross-file "$DXVK_SRC_DIR/$crossfile$1.txt" \
        --buildtype "release"                               \
        --prefix "$DXVK_BUILD_DIR"                          \
        $opt_strip                                          \
        --bindir "$2"                                      \
        --libdir "$2"                                      \
        -Db_ndebug=if-release                               \
        -Dbuild_id=$opt_buildid                             \
        "$DXVK_BUILD_DIR/build.$1"

  cd "$DXVK_BUILD_DIR/build.$1"
  ninja install

  if [ $opt_devbuild -eq 0 ]; then
    # get rid of some useless .a files
    rm "$DXVK_BUILD_DIR/$2/"*.!(dll)
    rm -R "$DXVK_BUILD_DIR/build.$1"
  fi
}

function package {
  cd "$DXVK_BUILD_DIR/.."
  tar -czf "$DXVK_ARCHIVE_PATH" "dxvk-$DXVK_VERSION"
  rm -R "dxvk-$DXVK_VERSION"
}

if [ $opt_32_only -eq 0 -a $opt_arm64x_only -eq 0 ]; then
  build_arch win64 x86
fi
if [ $opt_64_only -eq 0 -a $opt_arm64x_only -eq 0 ]; then
  build_arch win32 x32
fi
if [ $opt_build_arm64x -eq 1 -o $opt_arm64x_only -eq 1 ]; then
  build_arch arm64x arm64x
fi

if [ $opt_nopackage -eq 0 ]; then
  package
fi
