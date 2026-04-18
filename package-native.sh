#!/usr/bin/env bash

set -e

shopt -s extglob

if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: $0 version destdir [--no-package] [--dev-build] [--build-id] [--64-only] [--32-only] [--clang-btver2]"
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
opt_64_only=0
opt_32_only=0
opt_clang_btver2=0

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
  "--clang-btver2")
    opt_clang_btver2=1
    ;;
  *)
    echo "Unrecognized option: $1" >&2
    exit 1
  esac
  shift
done

if [ $opt_clang_btver2 -eq 1 ]; then
  CC=${CC:="clang"}
  CXX=${CXX:="clang++"}
else
  CC=${CC:="gcc"}
  CXX=${CXX:="g++"}
fi

function build_arch {  
  cd "$DXVK_SRC_DIR"

  opt_strip=
  if [ $opt_devbuild -eq 0 ]; then
    opt_strip=--strip
  fi

  local meson_args=(
    --buildtype "release"
    --prefix "$DXVK_BUILD_DIR/usr"
    --bindir "$2"
    --libdir "$2"
    -Dbuild_id=$opt_buildid
    --force-fallback-for=libdisplay-info
  )

  if [ $opt_clang_btver2 -eq 1 ]; then
    meson_args+=(-Db_lto=true)
  fi

  if [ -n "$opt_strip" ]; then
    meson_args+=("$opt_strip")
  fi

  local cflags="-m$1"
  local cxxflags="-m$1"
  local ldflags="-m$1"

  if [ $opt_clang_btver2 -eq 1 ]; then
    local tune_flags="-march=btver2 -mtune=btver2 -O3 -ffast-math -flto=full"
    cflags+=" $tune_flags"
    cxxflags+=" $tune_flags"
    ldflags+=" -flto=full"
  fi

  CC="$CC" CXX="$CXX" CFLAGS="$CFLAGS $cflags" CXXFLAGS="$CXXFLAGS $cxxflags" LDFLAGS="$LDFLAGS $ldflags" meson setup \
        "${meson_args[@]}" \
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

if [ $opt_32_only -eq 0 ]; then
  build_arch 64 lib
fi
if [ $opt_64_only -eq 0 ]; then
  build_arch 32 lib32
fi

if [ $opt_nopackage -eq 0 ]; then
  package
fi
