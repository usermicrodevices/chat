#include "Ipc.hpp"

IpcConnection::IpcConnection(asio::io_context& io, int fd)
: socket_(io) {
    socket_.assign(asio::local::stream_protocol(), fd);
}

IpcConnection::~IpcConnection() {
    close();
}

void IpcConnection::start(MessageHandler handler, ErrorHandler err_handler) {
    msg_handler_ = std::move(handler);
    err_handler_ = std::move(err_handler);
    readHeader();
}

void IpcConnection::readHeader() {
    auto self = shared_from_this();
    asio::async_read(socket_, asio::buffer(header_, 5),
                     [self](std::error_code ec, size_t) {
                         if (ec) {
                             if (self->err_handler_) self->err_handler_(ec);
                             return;
                         }
                         uint8_t cmd = self->header_[0];
                         uint32_t len = (static_cast<uint32_t>(self->header_[1])) |
                         (static_cast<uint32_t>(self->header_[2]) << 8) |
                         (static_cast<uint32_t>(self->header_[3]) << 16) |
                         (static_cast<uint32_t>(self->header_[4]) << 24);
                         self->readPayload(len, cmd);
                     });
}

void IpcConnection::readPayload(uint32_t len, uint8_t cmd) {
    auto self = shared_from_this();
    auto buf = std::make_shared<std::vector<uint8_t>>(len);
    asio::async_read(socket_, asio::buffer(*buf),
                     [self, buf, cmd](std::error_code ec, size_t) {
                         if (ec) {
                             if (self->err_handler_) self->err_handler_(ec);
                             return;
                         }
                         if (self->msg_handler_)
                             self->msg_handler_({cmd, std::move(*buf)});
                         self->readHeader();  // loop
                     });
}

void IpcConnection::asyncSend(IpcMessage msg, std::function<void(std::error_code)> cb) {
    uint32_t len = msg.payload.size();
    std::vector<uint8_t> frame;
    frame.push_back(msg.cmd);
    frame.push_back(static_cast<uint8_t>(len));
    frame.push_back(static_cast<uint8_t>(len >> 8));
    frame.push_back(static_cast<uint8_t>(len >> 16));
    frame.push_back(static_cast<uint8_t>(len >> 24));
    frame.insert(frame.end(), msg.payload.begin(), msg.payload.end());

    bool write_in_progress = !write_buf_.empty();
    write_buf_.insert(write_buf_.end(), frame.begin(), frame.end());
    if (!write_in_progress)
        doWrite();
}

void IpcConnection::doWrite() {
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(write_buf_),
                      [self](std::error_code ec, size_t) {
                          self->write_buf_.clear();
                          if (ec) {
                              if (self->err_handler_) self->err_handler_(ec);
                          }
                      });
}

void IpcConnection::close() {
    std::error_code ec;
    socket_.close(ec);
}
