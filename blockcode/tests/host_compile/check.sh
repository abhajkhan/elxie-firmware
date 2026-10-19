#!/usr/bin/env bash
# Type-checks every firmware source on a PC against stubs of the ESP-IDF v5.3
# (+ NimBLE, led_strip 3.x, esp_littlefs, U8g2) APIs it uses. Signatures in
# idf_stubs/ are copied from the real headers. Catches typos and type errors
# without the Xtensa toolchain — it does NOT replace `idf.py build`.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FW="$ROOT/firmware"
fail=0
n=0
for f in $(find "$FW/main" "$FW/components/elx_vm" "$ROOT/../testbench/main" -name '*.cpp'); do
  n=$((n + 1))
  if ! g++ -std=gnu++2b -O2 -c -o /dev/null -Wall -Wextra -Wformat-truncation=1 -Wno-format -Wno-unused-parameter -Wno-missing-field-initializers -Werror \
      -I"$FW/components/u8g2/csrc" -I"$ROOT/tests/host_compile/idf_stubs" -I"$FW/main" -I"$FW/components/elx_vm/include" "$f"; then
    echo "FAIL: ${f#$ROOT/}"; fail=1
  fi
done
echo "ESP-IDF type-check: $n files"
exit $fail
