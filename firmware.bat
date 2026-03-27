@echo off

:: Help statments if user asks
if "%~1" == "help" (
    echo Usage: firmware.bat [clean]
    echo Clean - removes build directory and sdkconfig file
)


:: Clean build directories when asked to
if "%~1" == "clean" (
    echo Cleaning build directory..
    rmdir /s /q "build"
    del /s /q "sdkconfig"
    echo Cleanup completed
)

:: Building firmware
echo Running ESP-IDF Docker Build Command...
docker run --rm -v %cd%:/project -w /project IDF_GIT_SAFE_DIR='/project' espressif/idf idf.py build

:: Spelling out next steps
echo Build Complete !!
echo Check build\ folder for binary file
echo Use/run 'esptool.py' to flash the firmware [build\EarCandy.bin] to the player

pause