#include "BinaryProtocol.hpp"
std::vector<uint8_t> BinaryMessage::serialize() const {
    uint32_t payload_len = htonl(payload.size());
    std::vector<uint8_t> result(sizeof(payload_len) + 1 + payload.size());
    memcpy(result.data(), &payload_len, 4);
    result[4] = static_cast<uint8_t>(type);
    if (!payload.empty())
        memcpy(result.data() + 5, payload.data(), payload.size());
    return result;
}
BinaryMessage BinaryMessage::deserialize(const uint8_t* data, size_t len) {
    if (len < 5) throw std::runtime_error("Truncated binary message");
    uint32_t payload_len;
    memcpy(&payload_len, data, 4);
    payload_len = ntohl(payload_len);
    BinaryMsgType type = static_cast<BinaryMsgType>(data[4]);
    std::vector<uint8_t> payload;
    if (payload_len > 0 && len >= 5 + payload_len)
        payload.assign(data + 5, data + 5 + payload_len);
    return {type, std::move(payload)};
}
void BinaryStream::async_read_header(asio::ip::tcp::socket& sock,
    std::function<void(std::error_code, uint32_t)> cb) {
    auto buf = std::make_shared<std::array<uint8_t, 4>>();
    asio::async_read(sock, asio::buffer(*buf),
        [buf, cb](std::error_code ec, size_t) {
            if (ec) { cb(ec, 0); return; }
            uint32_t len;
            memcpy(&len, buf->data(), 4);
            cb(ec, ntohl(len));
        });
}
void BinaryStream::async_read_payload(asio::ip::tcp::socket& sock, uint32_t len,
    std::function<void(std::error_code, std::vector<uint8_t>)> cb) {
    auto payload = std::make_shared<std::vector<uint8_t>>(len);
    asio::async_read(sock, asio::buffer(*payload),
        [payload, cb](std::error_code ec, size_t) {
            cb(ec, std::move(*payload));
        });
}
void BinaryStream::async_write(asio::ip::tcp::socket& sock, const BinaryMessage& msg,
    std::function<void(std::error_code)> cb) {
    auto data = std::make_shared<std::vector<uint8_t>>(msg.serialize());
    asio::async_write(sock, asio::buffer(*data),
        [data, cb](std::error_code ec, size_t) { cb(ec); });
}
