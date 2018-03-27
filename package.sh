#!/bin/bash

source release.sh

function package {
  tar -czf "$DXVK_ARCHIVE_PATH" -C "$DXVK_TARGET_DIR"/.. dxvk-$DXVK_VERSION
  rm -R "$DXVK_TARGET_DIR"
}

package
