@echo off

echo -e "Running ESP-IDF Docker Build Command..."
docker run --rm -v %cd%:/EarCandy -w /EarCandy espressif/idf idf.py build

echo -e "Build Complete !! \n Check build\ folder for binary file\nUse/run 'esptool.py' to flash the firmware [build\EarCandy.bin] to the player"

pause