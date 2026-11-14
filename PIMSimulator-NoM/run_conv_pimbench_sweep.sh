#!/usr/bin/env bash

set -Eeuo pipefail

CONFIG_FILE="system_hbm_64ch.ini"
LOG_DIR="${LOG_DIR:-logs}"
LOG="${LOG:-${LOG_DIR}/pimbench_conv_sweep_$(date -u +%Y%m%d_%H%M%S).log}"
REBUILD="${REBUILD:-0}"
BUILD_CMD="${BUILD_CMD:-scons}"
STOP_ON_ERROR="${STOP_ON_ERROR:-0}"

mkdir -p "$LOG_DIR"

exec > >(tee -a "$LOG") 2>&1

if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "Missing ${CONFIG_FILE}; run this script from the repository root."
    exit 1
fi

CONFIG_BACKUP="$(mktemp /tmp/system_hbm_64ch.ini.XXXXXX)"
cp "$CONFIG_FILE" "$CONFIG_BACKUP"

restore_config() {
    cp "$CONFIG_BACKUP" "$CONFIG_FILE"
    rm -f "$CONFIG_BACKUP"
}
trap restore_config EXIT

set_channels() {
    local channels="$1"

    echo
    echo "===== Setting NUM_CHANS=${channels} in ${CONFIG_FILE} ====="
    sed -i -E \
        "s/^NUM_CHANS[[:space:]]*=.*/NUM_CHANS=${channels}                        ; number of *logically independent* channels (i.e. each with a separate memory controller); should be a power of 2/" \
        "$CONFIG_FILE"
    grep -n "^NUM_CHANS" "$CONFIG_FILE"
}

rebuild_if_requested() {
    if [[ "$REBUILD" == "1" ]]; then
        echo
        echo "===== Rebuilding with: ${BUILD_CMD} ====="
        bash -lc "$BUILD_CMD"
    fi
}

run_case() {
    local label="$1"
    local depth="$2"
    local height="$3"
    local width="$4"
    local nom="$5"

    echo
    echo "===== $(date -u '+%Y-%m-%d %H:%M:%S UTC') | ${label} | D=${depth} H=${height} W=${width} NoM=${nom} ====="

    set +e
    if [[ "$nom" == "1" ]]; then
        PIM_CONV_D="$depth" PIM_CONV_H="$height" PIM_CONV_W="$width" PIM_CONV_NOM=1 \
            ./sim --gtest_filter=PIMBenchFixture.conv
    else
        PIM_CONV_D="$depth" PIM_CONV_H="$height" PIM_CONV_W="$width" \
            ./sim --gtest_filter=PIMBenchFixture.conv
    fi
    local status="$?"
    set -e

    echo "===== Finished ${label} NoM=${nom} exit_status=${status} ====="

    if [[ "$status" != "0" && "$STOP_ON_ERROR" == "1" ]]; then
        echo "Stopping because STOP_ON_ERROR=1."
        exit "$status"
    fi
}

echo "Writing full sweep log to: ${LOG}"
echo "Started at: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo "REBUILD=${REBUILD}"
echo "BUILD_CMD=${BUILD_CMD}"
echo "STOP_ON_ERROR=${STOP_ON_ERROR}"

set_channels 64
rebuild_if_requested
run_case "1. 64x448x448, 64 channels" 64 448 448 0
run_case "1. 64x448x448, 64 channels" 64 448 448 1
run_case "2. 64x224x224, 64 channels" 64 224 224 0
run_case "2. 64x224x224, 64 channels" 64 224 224 1
run_case "3. 128x224x224, 64 channels" 128 224 224 0
run_case "3. 128x224x224, 64 channels" 128 224 224 1

set_channels 16
rebuild_if_requested
run_case "4. 128x112x112, 16 channels" 128 112 112 0
run_case "4. 128x112x112, 16 channels" 128 112 112 1
run_case "5. 256x112x112, 16 channels" 256 112 112 0
run_case "5. 256x112x112, 16 channels" 256 112 112 1

set_channels 4
rebuild_if_requested
run_case "6. 256x56x56, 4 channels" 256 56 56 0
run_case "6. 256x56x56, 4 channels" 256 56 56 1
run_case "7. 512x56x56, 4 channels" 512 56 56 0
run_case "7. 512x56x56, 4 channels" 512 56 56 1

echo
echo "Completed at: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo "Full log: ${LOG}"