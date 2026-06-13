@echo off
set IDF_PATH=D:\Espressif\frameworks\esp-idf-v5.2.7
set IDF_TOOLS_PATH=D:\Espressif
set PYTHONUTF8=1
set PATH=D:\Espressif\tools\idf-python\3.11.2;D:\Espressif\tools\idf-python\3.11.2\Scripts;%PATH%
call D:\Espressif\frameworks\esp-idf-v5.2.7\export.bat
cd "E:\Github Project\RoboMind-S3"
echo === Build: SPI 20MHz + MISO=NC ===
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash
echo === Done! ===
pause
