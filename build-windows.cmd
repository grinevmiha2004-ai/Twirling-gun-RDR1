@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
if not exist dist mkdir dist
cl /nologo /std:c++17 /EHsc /W4 /WX /MT tests\double_tap_test.cpp /Febuild\double_tap_test.exe /Fobuild\double_tap_test.obj
if errorlevel 1 exit /b 1
build\double_tap_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /MT /O2 /LD src\plugin.cpp /Fobuild\plugin.obj /link /OUT:dist\TwirlingGun.asi
exit /b %errorlevel%
