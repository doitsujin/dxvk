FROM ubuntu:20.04

ENV TZ=US/Pacific
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && \
    echo $TZ > /etc/timezone

RUN apt update
RUN apt install -y sudo mingw-w64 wine64 wine64-tools python3-pip ninja-build git cmake

RUN pip3 install meson

RUN git clone --recursive https://github.com/KhronosGroup/glslang.git ~/glslang && \
    cd ~/glslang && \
    git checkout 8.13.3743 && \
    cmake . && \
    cmake --build . --config Release --target install

RUN useradd -ms /bin/bash docker && echo "docker:docker" | chpasswd && adduser docker sudo

USER docker
WORKDIR /home/docker
