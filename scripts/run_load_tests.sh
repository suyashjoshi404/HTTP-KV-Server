#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <server_host> <server_port> [output_file]" >&2
  exit 1
fi

HOST="$1"
PORT="$2"
RAW_OUTPUT_FILE="${3:-load_test_results.txt}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RESULTS_DIR="$PROJECT_ROOT/results"
mkdir -p "$RESULTS_DIR"

if [[ "$RAW_OUTPUT_FILE" == */* ]]; then
  OUTPUT_FILE="$RAW_OUTPUT_FILE"
else
  OUTPUT_FILE="$RESULTS_DIR/$RAW_OUTPUT_FILE"
fi

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

BINARY="$PROJECT_ROOT/build/load_generator"
TASKSET_CORES="2-5"

if [[ ! -x "$BINARY" ]]; then
  echo "load_generator binary not found at $BINARY" >&2
  exit 1
fi

# Change here the number of minutes needed for the load test
DURATION_SECONDS=$((5 * 60))
THINK_MS=0
KEY_SPACE=10000

touch "$OUTPUT_FILE"

# for CLIENTS in 1 2 3 4 8 16 32 64; do


# Determine block device for disk stats once. #
if [[ -z "${DEV_DEVICE:-}" ]]; then
  DEV_RAW=$(df --output=source "$PROJECT_ROOT" | tail -1)
  DEV_DEVICE=${DEV_RAW#/dev/}
  if ! grep -q "\b$DEV_DEVICE\b" /proc/diskstats; then
    echo "Warning: device '$DEV_DEVICE' not found in /proc/diskstats; disk metrics disabled" >&2
    DEV_DEVICE=""
  fi
fi


for CLIENTS in 2 3 4 5 6 7 8 10 12 14 16 32 64 128; do
  echo "Running clients=$CLIENTS duration=${DURATION_SECONDS}s read_ratio=$READ_RATIO" | tee -a "$OUTPUT_FILE"
  echo "CONFIG: clients=$CLIENTS duration=${DURATION_SECONDS}s read_ratio=$READ_RATIO key_space=$KEY_SPACE think_ms=$THINK_MS" | tee -a "$OUTPUT_FILE"

  # Capture starting metrics (core1 + disk) before launching clients.
  START_TIME=$(date +%s)
  CORE_START=($(awk '$1=="cpu1" {for(i=2;i<=NF;i++) printf "%s ",$i}' /proc/stat)) || CORE_START=()
  if [[ -n "$DEV_DEVICE" ]]; then
    DISK_START_LINE=$(awk -v d="$DEV_DEVICE" '$3==d || $2==d {print}' /proc/diskstats)
    read -r _ _ _ rc rm sr msr wc wm sw msw ios msio mswio <<<"$DISK_START_LINE" || true
    DISK_START_SR=${sr:-0}; DISK_START_SW=${sw:-0}; DISK_START_MSIO=${msio:-0}
  fi

  taskset -c "$TASKSET_CORES" "${BINARY}" "$HOST" "$PORT" <<EOF
$CLIENTS
$DURATION_SECONDS
$READ_RATIO
$KEY_SPACE
$THINK_MS
$OUTPUT_FILE
EOF

  # Capture ending metrics.
  END_TIME=$(date +%s)
  ELAPSED=$(( END_TIME - START_TIME ))
  CORE_END=($(awk '$1=="cpu1" {for(i=2;i<=NF;i++) printf "%s ",$i}' /proc/stat)) || CORE_END=()
  if [[ ${#CORE_START[@]} -gt 0 && ${#CORE_END[@]} -gt 0 ]]; then
    CORE_USER_DELTA=$(( CORE_END[0] - CORE_START[0] ))
    CORE_SYS_DELTA=$(( CORE_END[2] - CORE_START[2] ))
    CORE_TOTAL_DELTA=0
    for i in "${!CORE_END[@]}"; do
      CORE_TOTAL_DELTA=$(( CORE_TOTAL_DELTA + CORE_END[$i] - CORE_START[$i] ))
    done
    CORE_USER_PCT=$(awk -v u="$CORE_USER_DELTA" -v t="$CORE_TOTAL_DELTA" 'BEGIN {if(t>0) printf "%.2f", (u/t)*100; else print 0}')
    CORE_SYS_PCT=$(awk -v s="$CORE_SYS_DELTA" -v t="$CORE_TOTAL_DELTA" 'BEGIN {if(t>0) printf "%.2f", (s/t)*100; else print 0}')
  else
    CORE_USER_PCT="NA"; CORE_SYS_PCT="NA"
  fi

  if [[ -n "$DEV_DEVICE" ]]; then
    DISK_END_LINE=$(awk -v d="$DEV_DEVICE" '$3==d || $2==d {print}' /proc/diskstats)
    read -r _ _ _ rc rm sr msr wc wm sw msw ios msio mswio <<<"$DISK_END_LINE" || true
    DISK_END_SR=${sr:-0}; DISK_END_SW=${sw:-0}; DISK_END_MSIO=${msio:-0}
    D_SR=$(( DISK_END_SR - DISK_START_SR ))
    D_SW=$(( DISK_END_SW - DISK_START_SW ))
    D_MSIO=$(( DISK_END_MSIO - DISK_START_MSIO ))
    if [[ $ELAPSED -gt 0 ]]; then
      READ_KB_S=$(awk -v s="$D_SR" -v e="$ELAPSED" 'BEGIN {printf "%.2f", (s/2)/e}')
      WRITE_KB_S=$(awk -v s="$D_SW" -v e="$ELAPSED" 'BEGIN {printf "%.2f", (s/2)/e}')
      DISK_UTIL_PCT=$(awk -v ms="$D_MSIO" -v e="$ELAPSED" 'BEGIN {printf "%.2f", (ms/(e*1000))*100}')
    else
      READ_KB_S=0; WRITE_KB_S=0; DISK_UTIL_PCT=0
    fi
  else
    READ_KB_S="NA"; WRITE_KB_S="NA"; DISK_UTIL_PCT="NA"
  fi

  echo "METRICS: clients=$CLIENTS elapsed_s=$ELAPSED core1_user_pct=$CORE_USER_PCT core1_sys_pct=$CORE_SYS_PCT disk_read_kB_s=$READ_KB_S disk_write_kB_s=$WRITE_KB_S disk_util_pct=$DISK_UTIL_PCT" | tee -a "$OUTPUT_FILE"
  printf '%s\n' '---' | tee -a "$OUTPUT_FILE"
done

echo "All runs complete. Results in $OUTPUT_FILE"
