#pragma once

#include <cstdint>
#include <array>
#include <limits>

namespace nexus::book {

enum class Side : uint8_t {
    Buy = 0,
    Sell = 1,
    Unknown = 2
};

struct PriceLevel {
    uint32_t price{0};
    uint32_t quantity{0};
    uint32_t order_count{0};
};

struct BBO {
    uint32_t bid_price{0};
    uint32_t bid_shares{0};
    uint32_t ask_price{0};
    uint32_t ask_shares{0};

    [[nodiscard]] constexpr uint32_t spread() const noexcept {
        return (ask_price > bid_price) ? (ask_price - bid_price) : 0;
    }

    [[nodiscard]] constexpr bool is_crossed() const noexcept {
        return (bid_price != 0 && ask_price != 0 && bid_price >= ask_price);
    }
};

struct BookOrder {
    uint64_t order_reference_number{0};
    uint32_t price{0};
    uint32_t shares{0};
    Side     side{Side::Unknown};
    bool     active{false};
};

} // namespace nexus::book
