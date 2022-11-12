#!/usr/bin/env bash

# default directories
STEAMAPPS=${STEAMAPPS:-"$HOME/.steam/steam/steamapps"}
PROTON=${PROTON:-"$STEAMAPPS/common/Proton - Experimental"}
proton_dxvk=${proton_dxvk:-"files/lib/wine/dxvk"}
proton_dxvk64=${proton_dxvk64:-"files/lib64/wine/dxvk"}
dxvk_lib32=${dxvk_lib32:-"x32"}

# figure out where we are
basedir="$(dirname "$(readlink -f "$0")")"

# figure out which action to perform
action="$1"

case "$action" in
install)
  ;;
uninstall)
  ;;
*)
  echo "Unrecognized action: $action"
  echo "Usage: $0 [install|uninstall] [--app <steamappid>] [--no-proton] [--symlink]"
echo ""
  echo "To use custom Proton installation path, set \$PROTON or \$STEAMAPPS"
  exit 1
esac

# process arguments
shift

file_cmd="cp"

while (($# > 0)); do
  case "$1" in
  "--no-proton")
    unset PROTON
    ;;
  "--app")
    echo "Steam App ID: $2"
    export WINEPREFIX=$STEAMAPPS/compatdata/$2/pfx
    ;;
  "--symlink")
    file_cmd="ln -s"
    ;;
  esac
  shift
done

echo "Wine Prefix: $WINEPREFIX"
echo "Proton: $PROTON"

# check wine prefix before invoking wine, so that we
# don't accidentally create one if the user screws up
if [ -n "$WINEPREFIX" ] && ! [ -f "$WINEPREFIX/system.reg" ]; then
  echo "$WINEPREFIX:"' Not a valid wine prefix.' >&2
  exit 1
fi

if [ -n "$PROTON" ] && ! [ -f "$PROTON/proton" ]; then
  echo "$PROTON:"' Not a valid Proton installation.' >&2
  unset PROTON
fi

# find wine executable
export WINEDEBUG=-all
# disable mscoree and mshtml to avoid downloading
# wine gecko and mono
export WINEDLLOVERRIDES="mscoree,mshtml="

wine="wine"
wineboot="wineboot"

# $PATH is the way for user to control where wine is located (including custom Wine versions).
# Pure 64-bit Wine (non Wow64) requries skipping 32-bit steps.
# In such case, wine64 and winebooot will be present, but wine binary will be missing,
# however it can be present in other PATHs, so it shouldn't be used, to avoid versions mixing.
wine_path=$(dirname "$(which $wineboot)")
if ! [ -f "$wine_path/$wine" ]; then
    echo "Cannot find 32-bit wine. Exiting."
    exit 1
fi

# resolve 32-bit and 64-bit system32 path
winever=$($wine --version | grep wine)
if [ -z "$winever" ]; then
    echo "$wine:"' Not a wine executable. Check your $wine.' >&2
    exit 1
fi

# ensure wine placeholder dlls are recreated
# if they are missing
$wineboot -u

win32_sys_path=$($wine winepath -u 'C:\windows\system32' 2> /dev/null)
win32_sys_path="${win32_sys_path/$'\r'/}"

if [ -z "$win32_sys_path" ]; then
  echo 'Failed to resolve C:\windows\system32.' >&2
  exit 1
fi

# create native dll override
overrideDll() {
  $wine reg add 'HKEY_CURRENT_USER\Software\Wine\DllOverrides' /v $1 /d native /f >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo -e "Failed to add override for $1"
    exit 1
  fi
}

# remove dll override
restoreDll() {
  $wine reg delete 'HKEY_CURRENT_USER\Software\Wine\DllOverrides' /v $1 /f > /dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo "Failed to remove override for $1"
  fi
}

# copy or link dxvk dll, back up original file
installFile() {
  dstfile="${1}/${3}.dll"
  srcfile="${basedir}/${2}/${3}.dll"

  if [ -f "${srcfile}.so" ]; then
    srcfile="${srcfile}.so"
  fi

  if ! [ -f "${srcfile}" ]; then
    echo "${srcfile}: File not found. Skipping." >&2
    return 1
  fi

  if [ -n "$1" ]; then
    if [ -f "${dstfile}" ] || [ -h "${dstfile}" ]; then
      if ! [ -f "${dstfile}.old" ]; then
        mv "${dstfile}" "${dstfile}.old"
      else
        rm "${dstfile}"
      fi
      echo "${4}"
      $file_cmd "${srcfile}" "${dstfile}"
    else
      echo "${dstfile}: File not found in wine prefix" >&2
      return 1
    fi
  fi
  return 0
}

# installs a file that probably did not previously exist
installNewFile() {
  dstfile="${1}/${3}.dll"
  srcfile="${basedir}/${2}/${3}.dll"

  if [ -f "${srcfile}.so" ]; then
    srcfile="${srcfile}.so"
  fi

  if ! [ -f "${srcfile}" ]; then
    echo "${srcfile}: File not found. Skipping." >&2
    return 1
  fi

  if [ -n "$1" ]; then
    if [ -f "${dstfile}" ] || [ -h "${dstfile}" ]; then
      if ! [ -f "${dstfile}.old" ]; then
        mv "${dstfile}" "${dstfile}.old"
      else
        rm "${dstfile}"
      fi
    fi
    echo "${4}"
    $file_cmd "${srcfile}" "${dstfile}"
  fi
  return 0
}

# remove dxvk dll, restore original file
uninstallFile() {
  dstfile="${1}/${3}.dll"
  srcfile="${basedir}/${2}/${3}.dll"

  if [ -f "${srcfile}.so" ]; then
    srcfile="${srcfile}.so"
  fi

  if ! [ -f "${srcfile}" ]; then
    echo "${srcfile}: File not found. Skipping." >&2
    return 1
  fi

  if ! [ -f "${dstfile}" ] && ! [ -h "${dstfile}" ]; then
    echo "${dstfile}: File not found. Skipping." >&2
    return 1
  fi

  if [ -f "${dstfile}.old" ]; then
    echo "${4}"
    rm "${dstfile}"
    mv "${dstfile}.old" "${dstfile}"
    return 0
  else
    return 1
  fi
}

# remove file that may not have a .old
uninstallNewFile() {
  dstfile="${1}/${3}.dll"
  srcfile="${basedir}/${2}/${3}.dll"

  if [ -f "${srcfile}.so" ]; then
    srcfile="${srcfile}.so"
  fi

  if ! [ -f "${srcfile}" ]; then
    echo "${srcfile}: File not found. Skipping." >&2
    return 1
  fi

  if ! [ -f "${dstfile}" ] && ! [ -h "${dstfile}" ]; then
    echo "${dstfile}: File not found. Skipping." >&2
    return 1
  fi

  if [ -f "${dstfile}.old" ]; then
    echo "${4}"
    rm "${dstfile}"
    mv "${dstfile}.old" "${dstfile}"
    return 0
  else
    echo "${4}"
    rm "${dstfile}"
    return 0
  fi
}

install() {

  inst32_ret=-1
  installFile "$win32_sys_path" "$dxvk_lib32" "$1" "$1.dll -> c:/windows/system32/$1.dll"
  inst32_ret="$?"

  if (( ($inst32_ret == 0) )); then
    overrideDll "$1"
  fi
}

uninstall() {
  uninst32_ret=-1
  uninstallFile "$win32_sys_path" "$dxvk_lib32" "$1" "$1.dll.old -> c:/windows/system32/$1.dll"
  uninst32_ret="$?"

  if (( ($uninst32_ret == 0) )); then
    restoreDll "$1"
  fi
}

$action d3d9
$action d3d8

if [ "$action" == "install" ] && ! [ -z "$PROTON" ]; then
  # Install d3d8 and d3d9 to Proton
  installFile "$PROTON/$proton_dxvk" "$dxvk_lib32" "d3d9" "d3d9.dll -> \$PROTON/files/lib/wine/dxvk/d3d9.dll"
  installNewFile "$PROTON/$proton_dxvk" "$dxvk_lib32" "d3d8" "d3d8.dll -> \$PROTON/files/lib/wine/dxvk/d3d8.dll"
  installFile "$PROTON/$proton_dxvk64" "$dxvk_lib32" "d3d9" "d3d9.dll -> \$PROTON/files/lib64/wine/dxvk/d3d9.dll"
  installNewFile "$PROTON/$proton_dxvk64" "$dxvk_lib32" "d3d8" "d3d8.dll -> \$PROTON/files/lib64/wine/dxvk/d3d8.dll"

  # Update ./proton to install d8vk
  echo "Patching proton executable..."
  sed -i 's/dxvkfiles = \["d3d11", "d3d10core", "d3d9"\]/dxvkfiles = \["d3d11", "d3d10core", "d3d9", "d3d8"\]/' "$PROTON/proton"
elif  ! [ -z "$PROTON" ]; then
  # Uninstall d3d8/d3d9 from Proton
  uninstallFile "$PROTON/$proton_dxvk" "$dxvk_lib32" "d3d9" "d3d9.dll.old -> \$PROTON/files/lib/wine/dxvk/d3d9.dll"
  uninstallNewFile "$PROTON/$proton_dxvk" "$dxvk_lib32" "d3d8" "Removing \$PROTON/files/lib/wine/dxvk/d3d8.dll"
  uninstallFile "$PROTON/$proton_dxvk64" "$dxvk_lib32" "d3d9" "d3d9.dll.old -> \$PROTON/files/lib64/wine/dxvk/d3d9.dll"
  uninstallNewFile "$PROTON/$proton_dxvk64" "$dxvk_lib32" "d3d8" "Removing \$PROTON/files/lib64/wine/dxvk/d3d8.dll"

  # Revert ./proton to not install d8vk
  echo "Reverting proton executable..."
  sed -i 's/dxvkfiles = \["d3d11", "d3d10core", "d3d9", "d3d8"\]/dxvkfiles = \["d3d11", "d3d10core", "d3d9"\]/' "$PROTON/proton"
fi
