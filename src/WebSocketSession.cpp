#include "WebSocketSession.hpp"

std::atomic<uint64_t> WebSocketSession::next_id_{0};

WebSocketSession::WebSocketSession(asio::ip::tcp::socket socket)
    : socket_(std::move(socket)), id_(++next_id_) {}

uint64_t WebSocketSession::GetID()
{
    return id_;
}

void WebSocketSession::start(MessageHandler msg_handler, CloseHandler close_handler) {
    msg_handler_ = std::move(msg_handler);
    close_handler_ = std::move(close_handler);
    doHandshake();
}

void WebSocketSession::send(const nlohmann::json& msg) {
    if (!socket_.is_open()) return;
    std::string text = msg.dump();
    std::vector<uint8_t> frame;
    frame.push_back(0x81);
    if (text.size() < 126) {
        frame.push_back(static_cast<uint8_t>(text.size()));
    } else if (text.size() <= 0xFFFF) {
        frame.push_back(126);
        uint16_t len = htons(static_cast<uint16_t>(text.size()));
        frame.insert(frame.end(), reinterpret_cast<uint8_t*>(&len), reinterpret_cast<uint8_t*>(&len) + 2);
    } else {
        frame.push_back(127);
        uint64_t len = htobe64(text.size());
        frame.insert(frame.end(), reinterpret_cast<uint8_t*>(&len), reinterpret_cast<uint8_t*>(&len) + 8);
    }
    frame.insert(frame.end(), text.begin(), text.end());
    writeFrame(frame);
}

void WebSocketSession::close() {
    std::error_code ec;
    socket_.close(ec);
}

std::string WebSocketSession::computeAcceptKey(const std::string& key) {
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.size(), hash);
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        val = (val << 8) + hash[i];
        valb += 8;
        while (valb >= 0) {
            result.push_back(b64[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) result.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4) result.push_back('=');
    return result;
}

void WebSocketSession::doHandshake() {
    Logger::Trace("WebSocketSession::doHandshake");
    auto self = shared_from_this();
    asio::async_read_until(socket_, read_buf_, "\r\n\r\n",
    [self](std::error_code ec, size_t) {
        if (ec)
        {
            Logger::Error("WebSocketSession::doHandshake asio::async_read_until: {}", ec.message());
            return;
        }
        std::istream is(&self->read_buf_);
        std::string line, key;
        while (std::getline(is, line) && line != "\r") {
            if (line.find("Sec-WebSocket-Key:") == 0) {
                size_t pos = line.find(": ");
                key = line.substr(pos + 2);
                key.pop_back();
            }
        }
        if (key.empty()) return;
        std::string accept = self->computeAcceptKey(key);
        std::string response =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
        asio::async_write(self->socket_, asio::buffer(response),
        [self](std::error_code ec, size_t) {
            if (ec)
            {
                Logger::Error("WebSocketSession::doHandshake asio::async_write: {}", ec.message());
            }
            else
            {
                self->handshake_done_ = true;
                self->readFrame();
            }
        });
    });
}

void WebSocketSession::readFrame() {
    auto self = shared_from_this();
    asio::async_read(socket_, asio::buffer(header_buf_, 2),
    [self](std::error_code ec, size_t) {
        if (ec) { if (self->close_handler_) self->close_handler_(self); return; }
        uint8_t opcode = self->header_buf_[0] & 0x0F;
        bool masked = (self->header_buf_[1] & 0x80) != 0;
        uint64_t payload_len = self->header_buf_[1] & 0x7F;
        if (payload_len == 126) {
            asio::async_read(self->socket_, asio::buffer(self->header_buf_.data() + 2, 2),
            [self, opcode, masked](std::error_code ec, size_t) {
                if (ec) {
                    Logger::Error("WebSocketSession::readFrame (126): {}", ec.message());
                    if (self->close_handler_)
                        self->close_handler_(self);
                    return;
                }
                uint16_t len;
                memcpy(&len, self->header_buf_.data() + 2, 2);
                self->readPayload(ntohs(len), masked, opcode);
            });
        } else if (payload_len == 127) {
            asio::async_read(self->socket_, asio::buffer(self->header_buf_.data() + 2, 8),
            [self, opcode, masked](std::error_code ec, size_t) {
                if (ec) {
                    Logger::Error("WebSocketSession::readFrame (127): {}", ec.message());
                    if (self->close_handler_)
                        self->close_handler_(self);
                    return;
                }
                uint64_t len;
                memcpy(&len, self->header_buf_.data() + 2, 8);
                self->readPayload(be64toh(len), masked, opcode);
            });
        } else {
            self->readPayload(payload_len, masked, opcode);
        }
    });
}

void WebSocketSession::readPayload(uint64_t len, bool masked, uint8_t opcode) {
    auto self = shared_from_this();
    if (masked) {
        auto mask_key = std::make_shared<std::array<uint8_t, 4>>();
        asio::async_read(socket_, asio::buffer(*mask_key),
        [self, len, opcode, mask_key](std::error_code ec, size_t) {
            if (ec) { if (self->close_handler_) self->close_handler_(self); return; }
            auto payload = std::make_shared<std::vector<uint8_t>>(len);
            asio::async_read(self->socket_, asio::buffer(*payload),
            [self, payload, mask_key, opcode](std::error_code ec, size_t) {
                if (ec) { if (self->close_handler_) self->close_handler_(self); return; }
                for (size_t i = 0; i < payload->size(); ++i)
                    (*payload)[i] ^= (*mask_key)[i % 4];
                self->processPayload(payload, opcode);
            });
        });
    } else {
        auto payload = std::make_shared<std::vector<uint8_t>>(len);
        asio::async_read(socket_, asio::buffer(*payload),
        [self, payload, opcode](std::error_code ec, size_t) {
            if (ec) { if (self->close_handler_) self->close_handler_(self); return; }
            self->processPayload(payload, opcode);
        });
    }
}

void WebSocketSession::processPayload(std::shared_ptr<std::vector<uint8_t>> payload, uint8_t opcode) {
    if (opcode == 0x1) {
        handleTextMessage(std::string(payload->begin(), payload->end()));
        readFrame();
    } else if (opcode == 0x8) {
        std::vector<uint8_t> close_frame;
        close_frame.push_back(0x88);
        if (payload->size() < 126) {
            close_frame.push_back(static_cast<uint8_t>(payload->size()));
        } else if (payload->size() <= 0xFFFF) {
            close_frame.push_back(126);
            uint16_t len = htons(static_cast<uint16_t>(payload->size()));
            close_frame.insert(close_frame.end(), reinterpret_cast<uint8_t*>(&len), reinterpret_cast<uint8_t*>(&len) + 2);
        } else {
            close_frame.push_back(127);
            uint64_t len = htobe64(payload->size());
            close_frame.insert(close_frame.end(), reinterpret_cast<uint8_t*>(&len), reinterpret_cast<uint8_t*>(&len) + 8);
        }
        close_frame.insert(close_frame.end(), payload->begin(), payload->end());
        writeFrame(close_frame);
        close();
        if (close_handler_) close_handler_(shared_from_this());
    } else if (opcode == 0x9) {
        std::vector<uint8_t> pong_frame;
        pong_frame.push_back(0x8A);
        if (payload->size() < 126) {
            pong_frame.push_back(static_cast<uint8_t>(payload->size()));
        } else if (payload->size() <= 0xFFFF) {
            pong_frame.push_back(126);
            uint16_t len = htons(static_cast<uint16_t>(payload->size()));
            pong_frame.insert(pong_frame.end(), reinterpret_cast<uint8_t*>(&len), reinterpret_cast<uint8_t*>(&len) + 2);
        } else {
            pong_frame.push_back(127);
            uint64_t len = htobe64(payload->size());
            pong_frame.insert(pong_frame.end(), reinterpret_cast<uint8_t*>(&len), reinterpret_cast<uint8_t*>(&len) + 8);
        }
        pong_frame.insert(pong_frame.end(), payload->begin(), payload->end());
        writeFrame(pong_frame);
        readFrame();
    } else {
        readFrame();
    }
}

void WebSocketSession::writeFrame(const std::vector<uint8_t>& frame) {
    auto self = shared_from_this();
    auto data = std::make_shared<std::vector<uint8_t>>(frame);
    asio::async_write(socket_, asio::buffer(*data),
    [self, data](std::error_code ec, size_t) {
        if (ec) Logger::Error("WebSocketSession::writeFrame asio::async_write: {}", ec.message());
    });
}

void WebSocketSession::handleTextMessage(const std::string& text) {
    try {
        auto j = nlohmann::json::parse(text);
        if (msg_handler_) msg_handler_(shared_from_this(), j);
    } catch (...) {
        Logger::Warn("Invalid JSON from client {}; {}", id_, text);
    }
}

ChatWebSocketServer::ChatWebSocketServer(asio::io_context& io,
    const std::string& address, unsigned short port, ToMasterHandler to_master)
: io_(io),
acceptor_(io),
to_master_(to_master)
{
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(address), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    int optval = 1;
    if (::setsockopt(acceptor_.native_handle(), SOL_SOCKET, SO_REUSEPORT,
        &optval, sizeof(optval)) < 0) {
        Logger::Error("SO_REUSEPORT failed: {}", strerror(errno));
        throw std::runtime_error("SO_REUSEPORT failed");
    }
    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);
    Logger::Trace("WebSocket server listening on {}:{}", address, port);
}

void ChatWebSocketServer::start() {
    doAccept();
    Logger::Trace("WebSocket server listening on {}:{}",
                 acceptor_.local_endpoint().address().to_string(),
                 acceptor_.local_endpoint().port());
}

void ChatWebSocketServer::stop() {
    stopped_ = true;
    std::error_code ec;
    acceptor_.close(ec);
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& s : sessions_) s->close();
    sessions_.clear();
}

void ChatWebSocketServer::doAccept() {
    if (stopped_) return;
    acceptor_.async_accept(
    [this](std::error_code ec, asio::ip::tcp::socket socket) {
        if (!ec) {
            auto session = std::make_shared<WebSocketSession>(std::move(socket));
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_.insert(session);
            }
            session->start(
            [this](std::shared_ptr<WebSocketSession> s, nlohmann::json msg) {
                onMessage(s, msg);
            },
            [this](std::shared_ptr<WebSocketSession> s) {
                onClose(s);
            });
        }
        doAccept();
    });
}

void ChatWebSocketServer::broadcast(const nlohmann::json& msg) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& s : sessions_) {
        s->send(msg);
    }
}

void ChatWebSocketServer::onMessage(std::shared_ptr<WebSocketSession> session, nlohmann::json msg) {
    if (to_master_) to_master_(msg);
    Logger::Trace("ChatWebSocketServer::onMessage sessionID={}; message={}", session->GetID(), msg.dump());
}

void ChatWebSocketServer::onClose(std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session);
}
