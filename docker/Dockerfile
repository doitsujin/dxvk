FROM debian:buster-slim
RUN dpkg --add-architecture i386
RUN apt-get update && apt-get install -y curl apt-transport-https gnupg
RUN curl -o - https://dl.winehq.org/wine-builds/Release.key | apt-key add -
RUN echo "deb https://dl.winehq.org/wine-builds/debian/ buster main" > /etc/apt/sources.list.d/winehq.list
RUN apt-get update
RUN apt-get -y install winehq-staging meson mingw-w64 mingw-w64-i686-dev mingw-w64-x86-64-dev
RUN apt-get -y install build-essential unzip

RUN curl -L -o glslang.zip https://github.com/KhronosGroup/glslang/releases/download/7.8.2850/glslang-master-linux-Release.zip
RUN unzip glslang.zip -d /usr

RUN update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
RUN update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
RUN update-alternatives --set i686-w64-mingw32-gcc /usr/bin/i686-w64-mingw32-gcc-posix
RUN update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix

ADD . /root/build
WORKDIR /root/build
RUN mkdir -p /root/out/dxvk-master
ARG UID
ARG GID
RUN chown -R $UID:$GID /root
CMD ["/root/build/docker/rebuild.sh"]
