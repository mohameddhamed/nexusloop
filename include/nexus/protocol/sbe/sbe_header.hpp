#pragma once

#include <cstdint>

namespace nexus::protocol::sbe {

#pragma pack(push, 1)

struct MessageHeader {
    uint16_t block_length;
    uint16_t template_id;
    uint16_t schema_id;
    uint16_t version;
};

struct GroupSizeEncoding {
    uint16_t block_length;
    uint16_t num_in_group;
};

// SBE Market Data Incremental Refresh Entry
struct MarketDataEntry {
    int64_t  price_mantissa;
    int32_t  price_exponent;
    uint32_t order_qty;
    uint32_t security_id;
    uint8_t  update_action; // 0 = New, 1 = Change, 2 = Delete
    uint8_t  entry_type;    // '0' = Bid, '1' = Offer
    char     pad[2];
};

#pragma pack(pop)

static_assert(sizeof(MessageHeader) == 8, "MessageHeader must be exactly 8 bytes");
static_assert(sizeof(GroupSizeEncoding) == 4, "GroupSizeEncoding must be exactly 4 bytes");
static_assert(sizeof(MarketDataEntry) == 24, "MarketDataEntry must be exactly 24 bytes");

} // namespace nexus::protocol::sbe
