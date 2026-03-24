#!/bin/bash

printf "Running ESP-IDF Docker Build Command...\n"
docker run --rm -v $PWD:/EarCandy -w /EarCandy espressif/idf idf.py build

printf "Build Complete !! \n Check build/ folder for binary file\nUse \"esptool.py\" to flash the firmware [build/EarCandy.bin] to the player\n"