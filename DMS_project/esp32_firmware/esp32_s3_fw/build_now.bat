@echo off
set MSYSTEM=
call E:\trace\esp-ide\v5.3.5\esp-idf\export.bat > nul 2>&1
cd /d f:\netplus\keepeyesopen\DMS_project\esp32_firmware\esp32_s3_fw
idf.py build > f:\netplus\keepeyesopen\DMS_project\esp32_firmware\esp32_s3_fw\build_log.txt 2>&1
echo BUILD_EXIT_CODE=%ERRORLEVEL% >> f:\netplus\keepeyesopen\DMS_project\esp32_firmware\esp32_s3_fw\build_log.txt
