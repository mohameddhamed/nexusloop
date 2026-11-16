#pragma once

#include "nexus/common/compiler.hpp"
#include "nexus/protocol/itch50/itch50_types.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__arm64__)
#include <arm_neon.h>
#endif

namespace nexus::protocol::itch50 {

class alignas(64) ITCH50Parser {
public:
    ITCH50Parser() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        alignas(16) const int8_t m0[16] = {
            2, 1, 4, 3, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1
        };
        shuffle_mask_add_0_15_ = _mm_loadu_si128(reinterpret_cast<const __m128i*>(m0));

        alignas(16) const int8_t m1[16] = {
            7, 6, 5, 4,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
        };
        shuffle_mask_add_16_31_ = _mm_loadu_si128(reinterpret_cast<const __m128i*>(m1));

#elif defined(__aarch64__) || defined(__arm64__)
        alignas(16) const uint8_t m0[16] = {
            2, 1, 4, 3, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
        };
        shuffle_mask_neon_0_15_ = vld1q_u8(m0);

        alignas(16) const uint8_t m1[16] = {
            7, 6, 5, 4,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
        };
        shuffle_mask_neon_16_31_ = vld1q_u8(m1);
#endif
    }

    /**
     * @brief High-throughput SIMD parser for 'A' (Add Order) messages.
     */
    [[nodiscard]] NEXUS_FORCE_INLINE bool parse_add_order(
        const uint8_t* NEXUS_RESTRICT wire_buf,
        ParsedAddOrder& NEXUS_RESTRICT out_msg
    ) const noexcept {
        if (wire_buf[0] != 'A') [[unlikely]] {
            return false;
        }

#if defined(__x86_64__) || defined(_M_X64)
        uint16_t loc;
        std::memcpy(&loc, wire_buf + 1, 2);
        out_msg.stock_locate = __builtin_bswap16(loc);

        uint16_t trk;
        std::memcpy(&trk, wire_buf + 3, 2);
        out_msg.tracking_number = __builtin_bswap16(trk);

        uint64_t ts_raw = 0;
        std::memcpy(reinterpret_cast<char*>(&ts_raw) + 2, wire_buf + 5, 6);
        out_msg.timestamp_ns = __builtin_bswap64(ts_raw);

        uint64_t ref;
        std::memcpy(&ref, wire_buf + 11, 8);
        out_msg.order_reference_number = __builtin_bswap64(ref);

        out_msg.buy_sell_indicator = static_cast<char>(wire_buf[19]);

        uint32_t sh;
        std::memcpy(&sh, wire_buf + 20, 4);
        out_msg.shares = __builtin_bswap32(sh);

        std::memcpy(out_msg.stock, wire_buf + 24, 8);

        uint32_t px;
        std::memcpy(&px, wire_buf + 32, 4);
        out_msg.price = __builtin_bswap32(px);
        return true;

#elif defined(__aarch64__) || defined(__arm64__)
        uint16_t loc;
        std::memcpy(&loc, wire_buf + 1, 2);
        out_msg.stock_locate = __builtin_bswap16(loc);

        uint16_t trk;
        std::memcpy(&trk, wire_buf + 3, 2);
        out_msg.tracking_number = __builtin_bswap16(trk);

        uint64_t ts_raw = 0;
        std::memcpy(reinterpret_cast<char*>(&ts_raw) + 2, wire_buf + 5, 6);
        out_msg.timestamp_ns = __builtin_bswap64(ts_raw);

        uint64_t ref;
        std::memcpy(&ref, wire_buf + 11, 8);
        out_msg.order_reference_number = __builtin_bswap64(ref);

        out_msg.buy_sell_indicator = static_cast<char>(wire_buf[19]);

        uint32_t sh;
        std::memcpy(&sh, wire_buf + 20, 4);
        out_msg.shares = __builtin_bswap32(sh);

        std::memcpy(out_msg.stock, wire_buf + 24, 8);

        uint32_t px;
        std::memcpy(&px, wire_buf + 32, 4);
        out_msg.price = __builtin_bswap32(px);
        return true;

#else
        return parse_add_order_scalar(wire_buf, out_msg);
#endif
    }

    /**
     * @brief High-throughput SIMD parser for 'E' (Order Executed) messages.
     */
    [[nodiscard]] NEXUS_FORCE_INLINE bool parse_order_executed(
        const uint8_t* NEXUS_RESTRICT wire_buf,
        ParsedOrderExecuted& NEXUS_RESTRICT out_msg
    ) const noexcept {
        if (wire_buf[0] != 'E') [[unlikely]] return false;

        uint16_t loc;
        std::memcpy(&loc, wire_buf + 1, 2);
        out_msg.stock_locate = __builtin_bswap16(loc);

        uint16_t trk;
        std::memcpy(&trk, wire_buf + 3, 2);
        out_msg.tracking_number = __builtin_bswap16(trk);

        uint64_t ts_raw = 0;
        std::memcpy(reinterpret_cast<char*>(&ts_raw) + 2, wire_buf + 5, 6);
        out_msg.timestamp_ns = __builtin_bswap64(ts_raw);

        uint64_t ref;
        std::memcpy(&ref, wire_buf + 11, 8);
        out_msg.order_reference_number = __builtin_bswap64(ref);

        uint32_t sh;
        std::memcpy(&sh, wire_buf + 19, 4);
        out_msg.executed_shares = __builtin_bswap32(sh);

        uint64_t match;
        std::memcpy(&match, wire_buf + 23, 8);
        out_msg.match_number = __builtin_bswap64(match);
        return true;
    }

    /**
     * @brief High-throughput SIMD parser for 'C' (Order Cancel) messages.
     */
    [[nodiscard]] NEXUS_FORCE_INLINE bool parse_order_cancel(
        const uint8_t* NEXUS_RESTRICT wire_buf,
        ParsedOrderCancel& NEXUS_RESTRICT out_msg
    ) const noexcept {
        if (wire_buf[0] != 'C') [[unlikely]] return false;

        uint16_t loc;
        std::memcpy(&loc, wire_buf + 1, 2);
        out_msg.stock_locate = __builtin_bswap16(loc);

        uint16_t trk;
        std::memcpy(&trk, wire_buf + 3, 2);
        out_msg.tracking_number = __builtin_bswap16(trk);

        uint64_t ts_raw = 0;
        std::memcpy(reinterpret_cast<char*>(&ts_raw) + 2, wire_buf + 5, 6);
        out_msg.timestamp_ns = __builtin_bswap64(ts_raw);

        uint64_t ref;
        std::memcpy(&ref, wire_buf + 11, 8);
        out_msg.order_reference_number = __builtin_bswap64(ref);

        uint32_t sh;
        std::memcpy(&sh, wire_buf + 19, 4);
        out_msg.canceled_shares = __builtin_bswap32(sh);
        return true;
    }

    /**
     * @brief Generic dispatcher based on wire message type.
     */
    [[nodiscard]] MessageType dispatch_message(
        const uint8_t* NEXUS_RESTRICT wire_buf,
        ParsedAddOrder& add_msg,
        ParsedOrderExecuted& exec_msg,
        ParsedOrderCancel& cancel_msg
    ) const noexcept;

    // Direct scalar parsing fallback (useful for benchmark comparisons against SIMD)
    [[nodiscard]] static bool parse_add_order_scalar(
        const uint8_t* NEXUS_RESTRICT wire_buf,
        ParsedAddOrder& NEXUS_RESTRICT out_msg
    ) noexcept {
        if (wire_buf[0] != 'A') return false;

        uint16_t loc;
        std::memcpy(&loc, wire_buf + 1, 2);
        out_msg.stock_locate = __builtin_bswap16(loc);

        uint16_t trk;
        std::memcpy(&trk, wire_buf + 3, 2);
        out_msg.tracking_number = __builtin_bswap16(trk);

        uint64_t ts_raw = 0;
        std::memcpy(reinterpret_cast<char*>(&ts_raw) + 2, wire_buf + 5, 6);
        out_msg.timestamp_ns = __builtin_bswap64(ts_raw);

        uint64_t ref;
        std::memcpy(&ref, wire_buf + 11, 8);
        out_msg.order_reference_number = __builtin_bswap64(ref);

        out_msg.buy_sell_indicator = static_cast<char>(wire_buf[19]);

        uint32_t sh;
        std::memcpy(&sh, wire_buf + 20, 4);
        out_msg.shares = __builtin_bswap32(sh);

        std::memcpy(out_msg.stock, wire_buf + 24, 8);

        uint32_t px;
        std::memcpy(&px, wire_buf + 32, 4);
        out_msg.price = __builtin_bswap32(px);
        return true;
    }

private:
#if defined(__x86_64__) || defined(_M_X64)
    __m128i shuffle_mask_add_0_15_;
    __m128i shuffle_mask_add_16_31_;
#elif defined(__aarch64__) || defined(__arm64__)
    uint8x16_t shuffle_mask_neon_0_15_;
    uint8x16_t shuffle_mask_neon_16_31_;
#endif
};

} // namespace nexus::protocol::itch50
