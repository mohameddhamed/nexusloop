#include "nexus/protocol/itch50/itch50_parser.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

void create_synthetic_add_order(uint8_t* buf, uint64_t order_ref, uint32_t price, uint32_t shares, const char* symbol) {
    buf[0] = 'A'; // Message Type
    
    uint16_t locate = __builtin_bswap16(101);
    std::memcpy(buf + 1, &locate, 2);

    uint16_t tracking = __builtin_bswap16(202);
    std::memcpy(buf + 3, &tracking, 2);

    // 48-bit timestamp = 123456789000 nanoseconds
    uint64_t ts_raw = __builtin_bswap64(123456789000ULL);
    std::memcpy(buf + 5, reinterpret_cast<const char*>(&ts_raw) + 2, 6);

    uint64_t ref_be = __builtin_bswap64(order_ref);
    std::memcpy(buf + 11, &ref_be, 8);

    buf[19] = 'B'; // Buy

    uint32_t sh_be = __builtin_bswap32(shares);
    std::memcpy(buf + 20, &sh_be, 4);

    std::memset(buf + 24, ' ', 8);
    std::memcpy(buf + 24, symbol, std::strlen(symbol));

    uint32_t px_be = __builtin_bswap32(price);
    std::memcpy(buf + 32, &px_be, 4);
}

int main() {
    std::printf("--- RUNNING ITCH 5.0 PARSER UNIT TESTS ---\n");
    nexus::protocol::itch50::ITCH50Parser parser;

    uint8_t wire_buf[64]{0};
    create_synthetic_add_order(wire_buf, 987654321ULL, 1955000, 500, "AAPL");

    nexus::protocol::itch50::ParsedAddOrder simd_msg{};
    bool simd_ok = parser.parse_add_order(wire_buf, simd_msg);
    assert(simd_ok);

    nexus::protocol::itch50::ParsedAddOrder scalar_msg{};
    bool scalar_ok = nexus::protocol::itch50::ITCH50Parser::parse_add_order_scalar(wire_buf, scalar_msg);
    assert(scalar_ok);

    // Assert exact equivalency and fidelity
    assert(simd_msg.order_reference_number == 987654321ULL);
    assert(simd_msg.order_reference_number == scalar_msg.order_reference_number);
    assert(simd_msg.stock_locate == 101);
    assert(simd_msg.tracking_number == 202);
    assert(simd_msg.timestamp_ns == 123456789000ULL);
    assert(simd_msg.buy_sell_indicator == 'B');
    assert(simd_msg.shares == 500);
    assert(simd_msg.price == 1955000);
    assert(std::memcmp(simd_msg.stock, "AAPL    ", 8) == 0);

    std::printf("[PASS] AddOrder ('A') parsed and matched between SIMD and scalar!\n");

    // Test Order Executed ('E')
    uint8_t exec_buf[32]{0};
    exec_buf[0] = 'E';
    uint16_t loc_e = __builtin_bswap16(101);
    std::memcpy(exec_buf + 1, &loc_e, 2);
    uint16_t trk_e = __builtin_bswap16(202);
    std::memcpy(exec_buf + 3, &trk_e, 2);
    uint64_t ts_e = __builtin_bswap64(123456799000ULL);
    std::memcpy(exec_buf + 5, reinterpret_cast<const char*>(&ts_e) + 2, 6);
    uint64_t ref_e = __builtin_bswap64(987654321ULL);
    std::memcpy(exec_buf + 11, &ref_e, 8);
    uint32_t sh_e = __builtin_bswap32(200);
    std::memcpy(exec_buf + 19, &sh_e, 4);
    uint64_t match_e = __builtin_bswap64(555444333ULL);
    std::memcpy(exec_buf + 23, &match_e, 8);

    nexus::protocol::itch50::ParsedOrderExecuted exec_msg{};
    bool exec_ok = parser.parse_order_executed(exec_buf, exec_msg);
    assert(exec_ok);
    assert(exec_msg.order_reference_number == 987654321ULL);
    assert(exec_msg.executed_shares == 200);
    assert(exec_msg.match_number == 555444333ULL);

    std::printf("[PASS] OrderExecuted ('E') parsed successfully!\n");

    // Test Order Cancel ('C')
    uint8_t cancel_buf[32]{0};
    cancel_buf[0] = 'C';
    std::memcpy(cancel_buf + 1, &loc_e, 2);
    std::memcpy(cancel_buf + 3, &trk_e, 2);
    std::memcpy(cancel_buf + 5, reinterpret_cast<const char*>(&ts_e) + 2, 6);
    std::memcpy(cancel_buf + 11, &ref_e, 8);
    uint32_t sh_c = __builtin_bswap32(300);
    std::memcpy(cancel_buf + 19, &sh_c, 4);

    nexus::protocol::itch50::ParsedOrderCancel cancel_msg{};
    bool cancel_ok = parser.parse_order_cancel(cancel_buf, cancel_msg);
    assert(cancel_ok);
    assert(cancel_msg.order_reference_number == 987654321ULL);
    assert(cancel_msg.canceled_shares == 300);

    std::printf("[PASS] OrderCancel ('C') parsed successfully!\n");
    std::printf("All ITCH 5.0 parser unit tests passed!\n");
    return 0;
}
