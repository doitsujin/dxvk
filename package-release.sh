#!/bin/sh

set -e

usage() {
  echo "Usage: ${0#*/} version destdir [--no-package] [--dev-build] [--winelib]"
}

if [ -z "$1" ] || [ -z "$2" ]; then
  usage
  exit 1
fi

DXVK_VERSION="$1"
DXVK_SRC_DIR="$(readlink -f "$0")"; DXVK_SRC_DIR="${DXVK_SRC_DIR%/*}"
DXVK_BUILD_DIR="$(realpath "$2")/dxvk-${DXVK_VERSION}"
DXVK_ARCHIVE_PATH="${DXVK_BUILD_DIR}.tar.gz"

if [ -e "${DXVK_BUILD_DIR}" ]; then
  echo "Build directory ${DXVK_BUILD_DIR} already exists"
  exit 1
fi

shift 2

opt_nopackage=0
opt_devbuild=0
opt_winelib=0

crossfile="build-win"

for arg; do
  case "${arg}" in
    --no-package)
      opt_nopackage=1
      ;;
    --dev-build)
      opt_nopackage=1
      opt_devbuild=1
      ;;
    --winelib)
      opt_winelib=1
      crossfile="build-wine"
      ;;
    *)
      echo "Unrecognized option: ${arg}"
      usage
      exit 1
  esac
done

build_arch() {
  export WINEARCH="win$1"
  export WINEPREFIX="${DXVK_BUILD_DIR}/wine.$1"
  
  cd "${DXVK_SRC_DIR}"

  meson --cross-file "${DXVK_SRC_DIR}/${crossfile}${1}.txt" \
        --buildtype "release"                               \
        --prefix "${DXVK_BUILD_DIR}"                        \
        --strip                                             \
        --bindir "x$1"                                      \
        --libdir "x$1"                                      \
        -Denable_tests=false                                \
        "${DXVK_BUILD_DIR}/build.$1"

  cd "${DXVK_BUILD_DIR}/build.$1"
  ninja install

  if [ ${opt_devbuild} -eq 0 ]; then
    if [ ${opt_winelib} -eq 0 ]; then
      # get rid of some useless .a files
      find "${DXVK_BUILD_DIR}/x$1" -name '*.*' -a ! -name '.*' -a ! -name '*.dll' -exec rm \{\} \;
    fi
    rm -R "${DXVK_BUILD_DIR}/build.$1"
  fi
}

build_script() {
  cp "${DXVK_SRC_DIR}/setup_dxvk.sh" "${DXVK_BUILD_DIR}/setup_dxvk.sh"
  chmod +x "${DXVK_BUILD_DIR}/setup_dxvk.sh"
}

package() {
  cd "${DXVK_BUILD_DIR}/.."
  tar -czf "${DXVK_ARCHIVE_PATH}" "dxvk-${DXVK_VERSION}"
  rm -R "dxvk-${DXVK_VERSION}"
}

build_arch 64
build_arch 32
build_script

if [ ${opt_nopackage} -eq 0 ]; then
  package
fi
