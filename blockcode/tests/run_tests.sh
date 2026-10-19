#!/usr/bin/env bash
# Host tests for the VM core: assemble golden programs, run them on the real
# VM with a simulated robot, diff against expected traces, then fuzz the validator.
#   tests/run_tests.sh            run
#   tests/run_tests.sh --update   regenerate expected traces (review the diff!)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/tests/build"; mkdir -p "$BUILD"
VM="$ROOT/firmware/components/elx_vm"
CXX="${CXX:-g++}"
FLAGS="-std=c++17 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter -fsanitize=address,undefined -I$VM/include -DELX_WITH_NAMES"

python3 "$ROOT/tools/gen_opcodes.py" >/dev/null
$CXX $FLAGS -o "$BUILD/elx_sim" "$ROOT/tests/host/elx_sim.cpp" "$VM/vm.cpp" "$VM/program.cpp"
$CXX $FLAGS -o "$BUILD/test_validate" "$ROOT/tests/host/test_validate.cpp" "$VM/program.cpp"

declare -A ARGS=(
  [04_events]="--ms 5000 --stim touched.1@500:600 --stim touched.1@900:1000 --stim touched.1@2500:2600 --stim obstacle.10@3000:3100"
  [03_parallel]="--ms 10000"
  [09_compass]="--ms 4000 --sensor heading=300 --sensor magnet_strength=40 --stim magnet.0@2000:2100"
)
fail=0
for src in "$ROOT"/spec/test-vectors/*.elxa; do
  name="$(basename "$src" .elxa)"
  python3 "$ROOT/tools/elxasm.py" "$src" -o "$BUILD/$name.elxb" >/dev/null
  "$BUILD/elx_sim" "$BUILD/$name.elxb" ${ARGS[$name]:-} > "$BUILD/$name.trace"
  exp="$ROOT/spec/test-vectors/expected/$name.trace"
  if [[ "${1:-}" == "--update" ]]; then
    mkdir -p "$(dirname "$exp")"; cp "$BUILD/$name.trace" "$exp"; echo "updated $name"
  elif diff -u "$exp" "$BUILD/$name.trace"; then
    echo "PASS $name"
  else
    echo "FAIL $name"; fail=1
  fi
done
"$BUILD/test_validate" "$BUILD/02_control.elxb" || fail=1
exit $fail
