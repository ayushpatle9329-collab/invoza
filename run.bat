@echo off
cd /d "%~dp0"
echo [INVOZA] Compiling latest C++ backend...
g++ -std=c++17 backend\main.cpp -o backend\smartbill_server.exe -lws2_32
if errorlevel 1 (
  echo.
  echo ERROR: C++ compiler g++ was not found or compilation failed.
  echo Install MSYS2 UCRT64 and add C:\msys64\ucrt64\bin to PATH.
  pause
  exit /b 1
)
echo [INVOZA] Starting Smart-Bill backend on http://localhost:8080
echo Keep this window open while using the website.
backend\smartbill_server.exe
pause
