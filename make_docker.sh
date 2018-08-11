#!/bin/bash
mkdir -p out
cp docker/Dockerfile .
docker build . --build-arg UID=$UID --build-arg GID=$(id -g $USER) -t dxvk-build:latest
rm Dockerfile
docker run -u=$UID:$(id -g $USER) -it -v $(pwd)/out:/root/out dxvk-build:latest
