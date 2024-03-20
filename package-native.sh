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
DXVK_BUILD_DIR=$(realpath "$2")"/dxvk-native-$DXVK_VERSION"
DXVK_ARCHIVE_PATH=$(realpath "$2")"/dxvk-native-$DXVK_VERSION.tar.gz"

if [ -e "$DXVK_BUILD_DIR" ]; then
  echo "Build directory $DXVK_BUILD_DIR already exists"
  exit 1
fi

shift 2

opt_nopackage=0
opt_devbuild=0
opt_buildid=false

CC=${CC:="gcc"}
CXX=${CXX:="g++"}

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
  *)
    echo "Unrecognized option: $1" >&2
    exit 1
  esac
  shift
done

function build_arch {  
  cd "$DXVK_SRC_DIR"

  opt_strip=
  if [ $opt_devbuild -eq 0 ]; then
    opt_strip=--strip
  fi

  CC="$CC -m$1" CXX="$CXX -m$1" meson setup  \
        --buildtype "release"                \
        --prefix "$DXVK_BUILD_DIR/usr"       \
        $opt_strip                           \
        --bindir "$2"                        \
        --libdir "$2"                        \
        -Dbuild_id=$opt_buildid              \
        --force-fallback-for=libdisplay-info \
        "$DXVK_BUILD_DIR/build.$1"

  cd "$DXVK_BUILD_DIR/build.$1"
  ninja install

  if [ $opt_devbuild -eq 0 ]; then
    rm -r "$DXVK_BUILD_DIR/build.$1"
  fi
}

function package {
  cd "$DXVK_BUILD_DIR"
  tar -czf "$DXVK_ARCHIVE_PATH" "usr"
  cd ".."
  rm -R "dxvk-native-$DXVK_VERSION"
}

build_arch 64 lib
build_arch 32 lib32

if [ $opt_nopackage -eq 0 ]; then
  package
fi
