#!/usr/bin/env bash
cd "$(dirname "$0")"
if [ ! -f backend/smartbill_server ]; then
  g++ -std=c++17 backend/main.cpp -o backend/smartbill_server
fi
./backend/smartbill_server
