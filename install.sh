#!/usr/bin/env bash

# shellcheck source=https://raw.githubusercontent.com/nafigator/bash-helpers/1.1.2/src/bash-helpers.sh
if [[ -x /usr/local/lib/bash/includes/bash-helpers.sh ]]; then
  . /usr/local/lib/bash/includes/bash-helpers.sh
else
  source <(curl -s https://raw.githubusercontent.com/nafigator/bash-helpers/1.1.2/src/bash-helpers.sh)
fi

# Function for handling version flags
function print_version() {
	# shellcheck disable=SC2059
	printf "install.sh $(bold)${VERSION}$(clr)\n"
	# shellcheck disable=SC2059
	printf "bash-helpers.sh $(bold)${BASH_HELPERS_VERSION}$(clr)\n\n"
}

function usage_help() {
	# shellcheck disable=SC2059
  printf "$(bold)Usage:$(clr)
  install.sh [OPTIONS...] [COMMAND]

$(bold)Options:$(clr)
  -v, --version              Show script version
  -h, --help                 Show this help message
  -d, --debug                Run program in debug mode

$(bold)Environment variables:$(clr)
  WINEPREFIX                 Defines wine prefix to install DXVK libs. By default installs into current dir.

$(bold)Description:$(clr)
  Bash script for latest DXVK libs installation into wine prefixes.

  As dependencies it uses curl, grep, jq, cut and bash-helpers lib. See https://github.com/nafigator/bash-helpers.
  If bash-helpers not exists in bash includes than script sources it from github.

$(bold)Examples:$(clr)
  Installation into current dir:

    cd /home/user/.wine && install.sh

  Installation into defined prefix:

    WINEPREFIX=/home/user/.wine install.sh
"

	return 0
}

# shellcheck disable=SC2034
INTERACTIVE=1
# shellcheck disable=SC2034
VERSION=0.1.0

parse_options "${@}"
PARSE_RESULT=$?

[[ ${PARSE_RESULT} = 1 ]] && exit 1
[[ ${PARSE_RESULT} = 2 ]] && usage_help && exit 2

check_dependencies grep jq cut || exit 1

# Check environment variable for wine prefix
function check_env() {
  if [[ -z "$WINEPREFIX" ]]
  then
    readonly INSTALL_PATH="$(pwd)"
  else
    readonly INSTALL_PATH="$WINEPREFIX"
  fi

  debug "Variable INSTALL_PATH: $INSTALL_PATH"
}

# Check .reg file
function check_reg_file() {
  if [[ -z "$1" ]]; then
    error 'check_reg_file(): not found required parameters!'
    exit 1
  fi

  if [[ ! -s "$1" ]]; then
    error "Not found or empty file $1"
    exit 1
  fi

  if [[ ! -r "$1" ]]; then
    error "Unable to read file $1"
    exit 1
  fi
}

# Find prefix system bits info
function find_prefix_bits() {
  if [[ -z "$1" ]]; then
    error 'find_prefix_bits(): not found required parameters!'
    exit 1
  fi

  # Find out prefix bits
  if grep -q '#arch=win32' "$1"; then
    readonly SYSTEM_BITS=32
  elif grep -q '#arch=win64' "$1"; then
    readonly SYSTEM_BITS=64
  else
    error "Unable to detect system bits version"
    exit 1
  fi
}

# Receive info about latest release
function check_latest_release() {
  readonly release_api_url=https://api.github.com/repos/doitsujin/dxvk/releases/latest

  if RELEASE_LATEST_RESP="$(curl -sS -f --fail-early "$release_api_url" 2>&1)"; then
    debug "Variable RELEASE_LATEST_RESP: $RELEASE_LATEST_RESP"
  else
    error "Unable to receive latest release response: $RELEASE_LATEST_RESP"
    exit 1
  fi
}

# Download and prepare latest release
function prepare_release() {
  readonly jq_name_select='.assets[] | select(.content_type=="application/gzip").name'
  readonly jq_download_url='.assets[] | select(.content_type=="application/gzip").browser_download_url'
  readonly release_file_name="$(echo "$RELEASE_LATEST_RESP" | jq -r "$jq_name_select")"

  debug "Variable release_file_name: $release_file_name"

  if [[ -z "$release_file_name" ]]; then
    error "Unable to parse latest release file name"
    exit 1
  fi

  readonly archive_path="/tmp/$release_file_name"

  debug "Variable archive_path: $archive_path"

  if [[ ! -r "$archive_path" ]]; then
    readonly download_url="$(echo "$RELEASE_LATEST_RESP" | jq -r "$jq_download_url")"

    if ! download_resp="$(curl -s --fail-early --output-dir /tmp -LO "$download_url" 2>&1)"; then
      error "Download release failure: $download_resp"
      exit 1
    fi
  fi

  status "Release archive: $archive_path" OK

  if ! archive_root_dir="$(tar tf "$archive_path" 2>&1 | head -n1 | cut -d'/' -f1)"; then
    error "Unable to read archive: $archive_root_dir"
    exit 1
  fi

  debug "Variable archive_root_dir: $archive_root_dir"

  readonly RELEASE_PATH=/tmp/$archive_root_dir

  if [[ ! -d "$RELEASE_PATH" ]]; then
    if ! tar_res="$(tar zxf "$archive_path" -C /tmp 2>&1)"; then
      error "Tar failure: $tar_res"
      exit 1
    fi
  fi
}

function main() {
  check_env

  readonly reg_file="$INSTALL_PATH/system.reg"
  readonly sys_path="$INSTALL_PATH/drive_c/windows/system32"
  readonly wow_path="$INSTALL_PATH/drive_c/windows/syswow64"

  debug "Variable reg_file: $reg_file"
  debug "Variable sys_path: $sys_path"
  debug "Variable wow_path: $wow_path"

  check_reg_file "$reg_file"
  find_prefix_bits "$reg_file"

  status "$SYSTEM_BITS-bits prefix" OK

  check_latest_release

  status "Checkout latest release" OK

  prepare_release

  status "Prepare release" OK

  # Copy files
  if [[ "$SYSTEM_BITS" -eq 32 ]]; then
    if ! cp_res=$(cp "$RELEASE_PATH"/x32/*.dll "$sys_path" 2>&1); then
      error "Copy failure: $cp_res"
      exit 1
    fi

    status "Copy files" OK
    return 0
  fi

  if ! cp_res=$(cp "$RELEASE_PATH"/x64/*.dll "$sys_path" 2>&1); then
    error "Copy x64 files failure: $cp_res"
    exit 1
  fi

  if ! cp_res=$(cp "$RELEASE_PATH"/x32/*.dll "$wow_path" 2>&1); then
    error "Copy x32 files failure: $cp_res"
    exit 1
  fi

  status "Copy files" OK

  return 0
}

main "$@"
