#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <memory>

#include <asio.hpp>

enum class BinaryMsgType : uint8_t {
    CLIENT_MSG = 0x01,
    BROADCAST = 0x02,
    REGISTER = 0x03,
    UNREGISTER = 0x04,
    PING = 0x05,
    PONG = 0x06,
    ERROR = 0xFF
};

struct BinaryMessage {
    BinaryMsgType type;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> serialize() const;
    static BinaryMessage deserialize(const uint8_t* data, size_t len);
};

class BinaryStream {
public:
    static void async_read_header(asio::ip::tcp::socket& sock,
        std::function<void(std::error_code, uint32_t)> cb);
    static void async_read_payload(asio::ip::tcp::socket& sock, uint32_t len,
        std::function<void(std::error_code, std::vector<uint8_t>)> cb);
    static void async_write(asio::ip::tcp::socket& sock, const BinaryMessage& msg,
        std::function<void(std::error_code)> cb);
};
