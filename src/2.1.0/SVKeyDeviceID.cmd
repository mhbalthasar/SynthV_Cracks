@echo off

echo Invoking SVKey to retrieve device ID.
echo.

svkey.exe --get-device-id

echo.
echo Device ID should be printed above.

pause
