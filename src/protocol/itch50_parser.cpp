#include "nexus/protocol/itch50/itch50_parser.hpp"

namespace nexus::protocol::itch50 {

MessageType ITCH50Parser::dispatch_message(
    const uint8_t* NEXUS_RESTRICT wire_buf,
    ParsedAddOrder& add_msg,
    ParsedOrderExecuted& exec_msg,
    ParsedOrderCancel& cancel_msg
) const noexcept {
    uint8_t type = wire_buf[0];
    switch (type) {
        case 'A':
            if (parse_add_order(wire_buf, add_msg)) return MessageType::AddOrder;
            break;
        case 'E':
            if (parse_order_executed(wire_buf, exec_msg)) return MessageType::OrderExecuted;
            break;
        case 'C':
            if (parse_order_cancel(wire_buf, cancel_msg)) return MessageType::OrderCancel;
            break;
        default:
            break;
    }
    return MessageType::Unknown;
}

} // namespace nexus::protocol::itch50
