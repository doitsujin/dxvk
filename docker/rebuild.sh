#!/bin/bash
rm -rf /root/out/*
/root/build/package-release.sh master /root/out --no-package
