#!/usr/bin/env bash
cd "$(dirname "$0")"
echo "[INVOZA] Compiling latest C++ backend..."
g++ -std=c++17 -pthread backend/main.cpp -o backend/smartbill_server
./backend/smartbill_server
