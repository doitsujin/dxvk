#!/bin/bash

if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: package-release.sh version destdir [--no-package]"
  exit 1
fi

DXVK_VERSION="$1"
DXVK_SRC_DIR=`dirname $(readlink -f $0)`
DXVK_BUILD_DIR=$(realpath "$2")"/dxvk-$DXVK_VERSION"
DXVK_ARCHIVE_PATH=$(realpath "$2")"/dxvk-$DXVK_VERSION.tar.gz"

function build_arch {
  export WINEARCH="win$1"
  export WINEPREFIX="$DXVK_BUILD_DIR/wine.$1"
  
  cd "$DXVK_SRC_DIR"

  meson --cross-file "$DXVK_SRC_DIR/build-win$1.txt"  \
        --buildtype "release"                         \
        --prefix "$DXVK_BUILD_DIR/install.$1"         \
        --strip                                       \
        -Denable_tests=false                          \
        "$DXVK_BUILD_DIR/build.$1"

  cd "$DXVK_BUILD_DIR/build.$1"
  ninja install

  mkdir "$DXVK_BUILD_DIR/x$1"

  cp "$DXVK_BUILD_DIR/install.$1/bin/d3d11.dll" "$DXVK_BUILD_DIR/x$1/d3d11.dll"
  cp "$DXVK_BUILD_DIR/install.$1/bin/dxgi.dll" "$DXVK_BUILD_DIR/x$1/dxgi.dll"
  cp "$DXVK_BUILD_DIR/install.$1/bin/setup_dxvk.sh" "$DXVK_BUILD_DIR/x$1/setup_dxvk.sh"
  
  rm -R "$DXVK_BUILD_DIR/wine.$1"
  rm -R "$DXVK_BUILD_DIR/build.$1"
  rm -R "$DXVK_BUILD_DIR/install.$1"
}

function package {
  cd "$DXVK_BUILD_DIR/.."
  tar -czf "$DXVK_ARCHIVE_PATH" "dxvk-$DXVK_VERSION"
  rm -R "dxvk-$DXVK_VERSION"
}

build_arch 64
build_arch 32

if [ "$3" != "--no-package" ]; then
  package
fi
