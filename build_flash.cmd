@echo off
setlocal
echo === RoboMind-S3: esp_lcd ST7789 driver ===
set IDF_PATH=D:\Espressif\frameworks\esp-idf-v5.2.7
set IDF_TOOLS_PATH=D:\Espressif
set PYTHONUTF8=1
set PATH=D:\Espressif\tools\idf-python\3.11.2;D:\Espressif\tools\idf-python\3.11.2\Scripts;%PATH%
call D:\Espressif\frameworks\esp-idf-v5.2.7\export.bat
if errorlevel 1 goto fail
set IDF_CCACHE_ENABLE=0
cd /d "E:\Github Project\RoboMind-S3"
if errorlevel 1 goto fail
idf.py --no-ccache fullclean
if errorlevel 1 goto fail
idf.py --no-ccache set-target esp32s3
if errorlevel 1 goto fail
idf.py --no-ccache build
if errorlevel 1 goto fail
idf.py --no-ccache -p COM3 flash
if errorlevel 1 goto fail
echo === Done ===
pause
exit /b 0

:fail
echo === FAILED: errorlevel %errorlevel% ===
pause
exit /b %errorlevel%
