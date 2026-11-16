#!/usr/bin/env bash
# ==============================================================================
# Automated Sanitizer Suite: ASan, TSan, UBSan
# ==============================================================================

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

echo "================================================================="
echo "  COMPILING & EXECUTING UNDER ADDRESS & UB SANITIZERS (ASan/UBSan)"
echo "================================================================="
make clean
make SANITIZER=asan -j4
./bin/test_spsc_ring
./bin/test_itch50_parser
./bin/test_allocation_guard
./bin/test_zero_allocation_pipeline 1000000

echo ""
echo "================================================================="
echo "  COMPILING & EXECUTING UNDER THREAD SANITIZER (TSan)"
echo "================================================================="
make clean
make SANITIZER=tsan -j4
./bin/test_spsc_ring
./bin/test_mpmc_ring

echo ""
echo "================================================================="
echo "  ALL SANITIZER GATES CLEAN: ZERO RACES, LEAKS, OR UB DETECTED!   "
echo "================================================================="
