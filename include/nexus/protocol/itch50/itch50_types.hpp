#pragma once

#include <cstdint>
#include <array>
#include <string_view>
#include <cstring>

namespace nexus::protocol::itch50 {

#pragma pack(push, 1)

// Raw wire formats (Big-Endian Network Byte Order)
struct WireHeader {
    uint16_t length;
    uint8_t  message_type;
};

struct WireAddOrder {
    uint8_t  message_type;          // 'A'
    uint16_t stock_locate;          // Locate ID
    uint16_t tracking_number;       // Tracking number
    uint8_t  timestamp[6];          // 48-bit integer nanoseconds since midnight
    uint64_t order_reference_number;// Order ID
    char     buy_sell_indicator;    // 'B' / 'S'
    uint32_t shares;                // Quantity
    char     stock[8];              // Security ticker
    uint32_t price;                 // Fixed-point (4 decimals)
};

struct WireOrderExecuted {
    uint8_t  message_type;          // 'E'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_reference_number;
    uint32_t executed_shares;
    uint64_t match_number;
};

struct WireOrderCancel {
    uint8_t  message_type;          // 'C'
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t  timestamp[6];
    uint64_t order_reference_number;
    uint32_t canceled_shares;
};

#pragma pack(pop)

static_assert(sizeof(WireAddOrder) == 36, "WireAddOrder must be exactly 36 bytes");
static_assert(sizeof(WireOrderExecuted) == 31, "WireOrderExecuted must be exactly 31 bytes");
static_assert(sizeof(WireOrderCancel) == 23, "WireOrderCancel must be exactly 23 bytes");

// Decoded Host-Endian Structs (Optimized for cache locality and ALU processing)
struct ParsedAddOrder {
    uint64_t order_reference_number;
    uint64_t timestamp_ns;
    uint32_t price;
    uint32_t shares;
    uint16_t stock_locate;
    uint16_t tracking_number;
    char     stock[8];
    char     buy_sell_indicator;
    char     pad[7]; // Aligned to 48 bytes
};

struct ParsedOrderExecuted {
    uint64_t order_reference_number;
    uint64_t match_number;
    uint64_t timestamp_ns;
    uint32_t executed_shares;
    uint16_t stock_locate;
    uint16_t tracking_number;
};

struct ParsedOrderCancel {
    uint64_t order_reference_number;
    uint64_t timestamp_ns;
    uint32_t canceled_shares;
    uint16_t stock_locate;
    uint16_t tracking_number;
};

enum class MessageType : uint8_t {
    AddOrder = 'A',
    OrderExecuted = 'E',
    OrderCancel = 'C',
    Unknown = 0
};

} // namespace nexus::protocol::itch50
