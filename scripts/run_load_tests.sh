#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <server_host> <server_port> [output_file]" >&2
  exit 1
fi

HOST="$1"
PORT="$2"
OUTPUT_FILE="${3:-load_test_results.txt}"

read -r -p "Enter read ratio [0.0-1.0]: " READ_RATIO

while true; do
  if python3 - "$READ_RATIO" <<'PY'
import sys
try:
    val = float(sys.argv[1])
    assert 0.0 <= val <= 1.0
except Exception:
    sys.exit(1)
PY
  then
    break
  else
    read -r -p "Please enter a valid read ratio between 0.0 and 1.0: " READ_RATIO
  fi
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="$PROJECT_ROOT/build/load_generator"
TASKSET_CORES="2-5"

if [[ ! -x "$BINARY" ]]; then
  echo "load_generator binary not found at $BINARY" >&2
  exit 1
fi

DURATION_SECONDS=$((5 * 60))
THINK_MS=0
KEY_SPACE=10000

touch "$OUTPUT_FILE"

# for CLIENTS in 1 2 3 4 8 16 32 64; do


for CLIENTS in 16 32 64 128; do
  echo "Running clients=$CLIENTS duration=${DURATION_SECONDS}s read_ratio=$READ_RATIO" | tee -a "$OUTPUT_FILE"
  echo "CONFIG: clients=$CLIENTS duration=${DURATION_SECONDS}s read_ratio=$READ_RATIO key_space=$KEY_SPACE think_ms=$THINK_MS" | tee -a "$OUTPUT_FILE"
  taskset -c "$TASKSET_CORES" "${BINARY}" "$HOST" "$PORT" <<EOF
$CLIENTS
$DURATION_SECONDS
$READ_RATIO
$KEY_SPACE
$THINK_MS
$OUTPUT_FILE
EOF
  printf '%s\n' '---' | tee -a "$OUTPUT_FILE"
done

echo "All runs complete. Results in $OUTPUT_FILE"
