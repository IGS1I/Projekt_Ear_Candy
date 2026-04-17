#!/bin/bash

# clean build files if asked to
if [ "$1" == "clean" ]; then
    printf "Cleaning build folder..\n"
    rm -rf ./build sdkconfig
fi

# Building firmware
printf "Running ESP-IDF Docker Build Command...\n"
if docker run --rm -v $PWD:/project -w /project -e IDF_GIT_SAFE_DIR='/project' espressif/idf:latest idf.py build; then
    # Telling next steps
    printf "\nBuild Complete !!\nCheck build/ folder for binary (.bin) file. The .elf file is for debugging and stepping through the firmware.\nUse \"esptool.py\" to flash the firmware [build/EarCandy.bin] to the player\n"
else
    # If the build fails, print an error message and exit
    printf "\nBuild Failed !!\nPlease check the error messages above and fix them before trying again.\n"
    exit 1
fi