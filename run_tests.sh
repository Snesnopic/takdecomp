#!/bin/bash
set -e

# Colori per il terminale
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "====== [1/2] Compilazione del Progetto ======"
if [ ! -d "build" ]; then
    mkdir build
fi
if [ ! -f "build/Makefile" ]; then
    cd build && cmake .. && cd ..
fi
make -C build

echo -e "\n====== [2/2] Esecuzione dei Test E2E ======"
if python3 tests/run_e2e_tests.py; then
    echo -e "\n${GREEN}✔ TUTTI I TEST SONO PASSATI CON SUCCESSO!${NC}"
    exit 0
else
    echo -e "\n${RED}✘ ALCUNI TEST SONO FALLITI!${NC}"
    exit 1
fi
