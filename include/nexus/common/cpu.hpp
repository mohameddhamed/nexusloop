#pragma once

#include "nexus/common/compiler.hpp"
#include <pthread.h>
#include <cstdint>

namespace nexus::common {

/**
 * @brief Pin a thread to a specific hardware execution core and optionally elevate to real-time priority.
 */
bool affinitize_thread(pthread_t thread, int core_id, int priority = 99) noexcept;

/**
 * @brief Pin the calling thread to a specific hardware execution core.
 */
bool affinitize_current_thread(int core_id, int priority = 99) noexcept;

/**
 * @brief Query current hardware CPU core ID executing this thread.
 */
[[nodiscard]] int get_current_core() noexcept;

} // namespace nexus::common
