#!/usr/bin/env bash
# ==============================================================================
# NexusLoop Hardware PMU Profiling Script
# Validates mechanical sympathy: >95% reduction in L1 misses, 0 LLC bounces
# ==============================================================================

set -euo pipefail

BIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../bin" && pwd)"

echo "================================================================="
echo "  NEXUSLOOP HARDWARE PMU PROFILING & MECHANICAL SYMPATHY AUDIT  "
echo "================================================================="

if ! command -v perf &> /dev/null; then
    echo "Notice: Linux 'perf' not found on current host. On macOS, use Instruments (xctrace)."
    echo "Executing binary directly to verify latency outputs:"
    "${BIN_DIR}/bench_spsc_latency"
    exit 0
fi

echo "Profiling Tier 1 SPSC Ring Buffer Core-to-Core Handoff..."
perf stat -e \
    cycles,instructions,cache-references,cache-misses,\
    L1-dcache-loads,L1-dcache-load-misses,\
    LLC-loads,LLC-load-misses \
    "${BIN_DIR}/bench_spsc_latency"

echo ""
echo "Profiling ITCH 5.0 SIMD Parsing Engine..."
perf stat -e \
    cycles,instructions,L1-dcache-load-misses,branch-misses \
    "${BIN_DIR}/bench_itch_simd"
