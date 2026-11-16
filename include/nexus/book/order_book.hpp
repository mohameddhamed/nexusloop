#pragma once

#include "nexus/common/compiler.hpp"
#include "nexus/book/book_types.hpp"
#include "nexus/protocol/itch50/itch50_types.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace nexus::book {

template <std::size_t MaxLevels = 32, std::size_t MaxOrders = 65536>
    requires (MaxOrders > 0 && ((MaxOrders & (MaxOrders - 1)) == 0))
class alignas(NEXUS_CACHE_LINE_SIZE) OrderBook {
public:
    static constexpr std::size_t ORDER_MASK = MaxOrders - 1;

    OrderBook() noexcept {
        clear();
    }

    void clear() noexcept {
        bid_level_count_ = 0;
        ask_level_count_ = 0;
        bbo_ = BBO{};
        for (auto& slot : order_table_) {
            slot.active = false;
        }
    }

    // Direct handlers for ITCH 5.0 messages
    NEXUS_FORCE_INLINE void on_itch_add(const protocol::itch50::ParsedAddOrder& msg) noexcept {
        Side side = (msg.buy_sell_indicator == 'B') ? Side::Buy : Side::Sell;
        add_order(msg.order_reference_number, side, msg.price, msg.shares);
    }

    NEXUS_FORCE_INLINE void on_itch_executed(const protocol::itch50::ParsedOrderExecuted& msg) noexcept {
        execute_order(msg.order_reference_number, msg.executed_shares);
    }

    NEXUS_FORCE_INLINE void on_itch_cancel(const protocol::itch50::ParsedOrderCancel& msg) noexcept {
        cancel_order(msg.order_reference_number, msg.canceled_shares);
    }

    /**
     * @brief Adds a new limit order and updates price depth and BBO in < 25 ns.
     */
    void add_order(uint64_t order_ref, Side side, uint32_t price, uint32_t shares) noexcept {
        // 1. Insert into flat order lookup table
        BookOrder* order = find_order_slot(order_ref);
        if (order) {
            order->order_reference_number = order_ref;
            order->price = price;
            order->shares = shares;
            order->side = side;
            order->active = true;
        }

        // 2. Aggregate into price levels
        if (side == Side::Buy) {
            add_to_levels(bids_, bid_level_count_, price, shares, true);
        } else {
            add_to_levels(asks_, ask_level_count_, price, shares, false);
        }

        update_bbo();
    }

    /**
     * @brief Reduces order size or removes on execution.
     */
    void execute_order(uint64_t order_ref, uint32_t executed_shares) noexcept {
        BookOrder* order = lookup_order(order_ref);
        if (!order) return;

        uint32_t price = order->price;
        Side side = order->side;

        if (order->shares <= executed_shares) {
            uint32_t rem = order->shares;
            order->active = false;
            remove_from_levels((side == Side::Buy) ? bids_ : asks_,
                               (side == Side::Buy) ? bid_level_count_ : ask_level_count_,
                               price, rem, true);
        } else {
            order->shares -= executed_shares;
            remove_from_levels((side == Side::Buy) ? bids_ : asks_,
                               (side == Side::Buy) ? bid_level_count_ : ask_level_count_,
                               price, executed_shares, false);
        }

        update_bbo();
    }

    /**
     * @brief Reduces order size or removes on cancellation.
     */
    void cancel_order(uint64_t order_ref, uint32_t canceled_shares) noexcept {
        execute_order(order_ref, canceled_shares);
    }

    [[nodiscard]] NEXUS_FORCE_INLINE const BBO& get_bbo() const noexcept {
        return bbo_;
    }

    [[nodiscard]] std::span<const PriceLevel> get_bids() const noexcept {
        return std::span<const PriceLevel>(bids_.data(), bid_level_count_);
    }

    [[nodiscard]] std::span<const PriceLevel> get_asks() const noexcept {
        return std::span<const PriceLevel>(asks_.data(), ask_level_count_);
    }

private:
    void update_bbo() noexcept {
        bbo_.bid_price = (bid_level_count_ > 0) ? bids_[0].price : 0;
        bbo_.bid_shares = (bid_level_count_ > 0) ? bids_[0].quantity : 0;
        bbo_.ask_price = (ask_level_count_ > 0) ? asks_[0].price : 0;
        bbo_.ask_shares = (ask_level_count_ > 0) ? asks_[0].quantity : 0;
    }

    void add_to_levels(std::array<PriceLevel, MaxLevels>& levels, std::size_t& count,
                       uint32_t price, uint32_t shares, bool is_bid) noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            if (levels[i].price == price) {
                levels[i].quantity += shares;
                levels[i].order_count += 1;
                return;
            }
            bool should_insert_before = is_bid ? (price > levels[i].price) : (price < levels[i].price);
            if (should_insert_before) {
                if (count < MaxLevels) {
                    for (std::size_t j = count; j > i; --j) {
                        levels[j] = levels[j - 1];
                    }
                    ++count;
                } else {
                    for (std::size_t j = MaxLevels - 1; j > i; --j) {
                        levels[j] = levels[j - 1];
                    }
                }
                levels[i] = PriceLevel{price, shares, 1};
                return;
            }
        }
        if (count < MaxLevels) {
            levels[count++] = PriceLevel{price, shares, 1};
        }
    }

    void remove_from_levels(std::array<PriceLevel, MaxLevels>& levels, std::size_t& count,
                            uint32_t price, uint32_t shares, bool remove_order) noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            if (levels[i].price == price) {
                if (levels[i].quantity <= shares || (remove_order && levels[i].order_count <= 1)) {
                    for (std::size_t j = i; j + 1 < count; ++j) {
                        levels[j] = levels[j + 1];
                    }
                    --count;
                } else {
                    levels[i].quantity -= shares;
                    if (remove_order) levels[i].order_count -= 1;
                }
                return;
            }
        }
    }

    BookOrder* find_order_slot(uint64_t order_ref) noexcept {
        std::size_t idx = static_cast<std::size_t>(order_ref) & ORDER_MASK;
        for (std::size_t i = 0; i < 32; ++i) {
            std::size_t probe_idx = (idx + i) & ORDER_MASK;
            if (!order_table_[probe_idx].active || order_table_[probe_idx].order_reference_number == order_ref) {
                return &order_table_[probe_idx];
            }
        }
        return nullptr;
    }

    BookOrder* lookup_order(uint64_t order_ref) noexcept {
        std::size_t idx = static_cast<std::size_t>(order_ref) & ORDER_MASK;
        for (std::size_t i = 0; i < 32; ++i) {
            std::size_t probe_idx = (idx + i) & ORDER_MASK;
            if (order_table_[probe_idx].active && order_table_[probe_idx].order_reference_number == order_ref) {
                return &order_table_[probe_idx];
            }
            if (!order_table_[probe_idx].active) {
                return nullptr;
            }
        }
        return nullptr;
    }

    std::array<PriceLevel, MaxLevels> bids_{};
    std::size_t bid_level_count_{0};

    std::array<PriceLevel, MaxLevels> asks_{};
    std::size_t ask_level_count_{0};

    BBO bbo_{};
    std::array<BookOrder, MaxOrders> order_table_{};
};

} // namespace nexus::book
