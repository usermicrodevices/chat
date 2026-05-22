#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include <asio.hpp>

#include "Logger.hpp"

struct IpcMessage {
    uint8_t cmd;
    std::vector<uint8_t> payload;
};

class IpcConnection : public std::enable_shared_from_this<IpcConnection> {
public:
    using MessageHandler = std::function<void(IpcMessage)>;
    using ErrorHandler = std::function<void(std::error_code)>;

    explicit IpcConnection(asio::io_context& io, int fd);
    ~IpcConnection();

    void start(MessageHandler handler, ErrorHandler err_handler);
    void send(IpcMessage msg);
    void close();

private:
    asio::local::stream_protocol::socket socket_;
    uint8_t header_[5];
    std::vector<uint8_t> payload_buf_;
    MessageHandler msg_handler_;
    ErrorHandler err_handler_;
    bool writing_ = false;
    std::vector<uint8_t> write_buf_;

    void readHeader();
    void readPayload(uint32_t len, uint8_t cmd);
    void doWrite();
};
