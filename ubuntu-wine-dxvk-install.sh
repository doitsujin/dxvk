#/bin/bash
#
# Tested only on (K)Ubuntu 18.04 may work on other Ubuntu versions as well and very likely work on all kinds of distros based on Ubuntu 18.04.
#
# This script will install wine staging and setup dxvk in the default .wine folder in the home directory. Run again to build the last cutting edge DXVK
#
# install wine staging via https://wiki.winehq.org/Ubuntu
cd /tmp
sudo dpkg --add-architecture i386
wget -nc https://dl.winehq.org/wine-builds/Release.key
sudo apt-key add Release.key
sudo apt-add-repository https://dl.winehq.org/wine-builds/ubuntu/
sudo apt install -y winehq-staging
# show installed version
wine --version
# install meson (only staight forward thing here sadly)
sudo apt install -y meson
# install specific versions of mingw needed to build dxvk
sudo apt install -y mingw-w64
printf "$(tput bold)\n>>>>>> Select posix on all the following dialogs. <<<<<<\n\n$(tput sgr 0)"
sudo update-alternatives --config i686-w64-mingw32-g++
sudo update-alternatives --config i686-w64-mingw32-gcc
sudo update-alternatives --config x86_64-w64-mingw32-g++
sudo update-alternatives --config x86_64-w64-mingw32-gcc
# install glslang
wget https://github.com/KhronosGroup/glslang/releases/download/7.8.2850/glslang-master-linux-Release.zip
sudo unzip glslang-master-linux-Release.zip -d /usr
# Build Dxvk
if cd ~/dxvk; then
	git pull
else
	git clone https://github.com/doitsujin/dxvk.git ~/dxvk
	cd ~/dxvk
fi
./package-release.sh master ~/dxvk-target --no-package

# set wine to use dxvk
cd ~/dxvk-target/dxvk-master/x32
WINEPREFIX=~/.wine bash setup_dxvk.sh
cd ~/dxvk-target/dxvk-master/x64
WINEPREFIX=~./wine bash setup_dxvk.sh
