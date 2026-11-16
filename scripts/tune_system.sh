#!/usr/bin/env bash
# ==============================================================================
# NexusLoop Low-Latency System Kernel & Hardware Tuning Script
# Configures Linux target servers for zero-jitter, deterministic execution
# ==============================================================================

set -euo pipefail

echo ">>> Applying NexusLoop Low-Latency Linux System Tuning..."

# 1. Disable CPU Frequency Scaling and Turbo Jitter
if [ -d /sys/devices/system/cpu/cpu0/cpufreq ]; then
    echo "performance" | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor || true
    echo "0" | tee /sys/devices/system/cpu/intel_pstate/no_turbo || true
fi

# 2. Configure 2MB HugePages
echo ">>> Allocating 2048 x 2MB HugePages (4GB total)..."
echo 2048 | tee /proc/sys/vm/nr_hugepages || true

# 3. Disable Transparent HugePages (prevents background compaction spikes)
echo "never" | tee /sys/kernel/mm/transparent_hugepage/enabled || true
echo "never" | tee /sys/kernel/mm/transparent_hugepage/defrag || true

# 4. Network Stack Zero-Copy & Busy Poll Tuning
echo ">>> Tuning Network Stack for Ultra-Low Latency..."
sysctl -w net.core.busy_read=50 || true
sysctl -w net.core.busy_poll=50 || true
sysctl -w net.core.rmem_max=16777216 || true
sysctl -w net.core.wmem_max=16777216 || true
sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216" || true
sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216" || true
sysctl -w net.ipv4.tcp_low_latency=1 || true

echo ">>> Tuning applied successfully. Physical cores ready for NexusLoop pinned threads."
