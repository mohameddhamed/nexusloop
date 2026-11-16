#pragma once

#include <cstddef>
#include <new>

// Cache line size for mechanical sympathy and false-sharing prevention
#if defined(__x86_64__) || defined(_M_X64)
inline constexpr std::size_t NEXUS_CACHE_LINE_SIZE = 64;
#elif defined(__aarch64__) || defined(__arm64__)
// Apple Silicon and Neoverse cores often use 128-byte cache lines
inline constexpr std::size_t NEXUS_CACHE_LINE_SIZE = 128;
#else
inline constexpr std::size_t NEXUS_CACHE_LINE_SIZE = 64;
#endif

// Compiler-specific forceinline and restrict macros
#if defined(_MSC_VER)
    #define NEXUS_FORCE_INLINE __forceinline
    #define NEXUS_NOINLINE __declspec(noinline)
    #define NEXUS_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
    #define NEXUS_FORCE_INLINE inline __attribute__((always_inline))
    #define NEXUS_NOINLINE __attribute__((noinline))
    #define NEXUS_RESTRICT __restrict__
#else
    #define NEXUS_FORCE_INLINE inline
    #define NEXUS_NOINLINE
    #define NEXUS_RESTRICT
#endif

// Branch prediction hints (in addition to C++20 [[likely]] / [[unlikely]])
#if defined(__GNUC__) || defined(__clang__)
    #define NEXUS_BUILTIN_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define NEXUS_BUILTIN_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define NEXUS_BUILTIN_LIKELY(x)   (x)
    #define NEXUS_BUILTIN_UNLIKELY(x) (x)
#endif
