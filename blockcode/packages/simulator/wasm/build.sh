#!/usr/bin/env bash
# Builds the robot VM for the browser. Requires Emscripten (emsdk) on PATH.
set -euo pipefail
cd "$(dirname "$0")"
VM=../../../firmware/components/elx_vm
mkdir -p ../dist
emcc -O2 -std=c++17 -I$VM/include sim_bridge.cpp $VM/vm.cpp $VM/program.cpp \
  --js-library host_imports.js \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createElxieVm -sENVIRONMENT=web,node \
  -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=1048576 \
  -sEXPORTED_FUNCTIONS='["_vm_upload_buffer","_vm_load","_vm_start","_vm_stop","_vm_tick","_vm_running","_vm_num_threads","_vm_thread_state","_vm_thread_pc","_vm_num_vars","_vm_var","_vm_tick_ms","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["HEAPU8","HEAPF32","HEAPU32"]' \
  -o ../dist/elxie-vm.js
echo "built ../dist/elxie-vm.js + .wasm"
