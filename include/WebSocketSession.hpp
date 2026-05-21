#pragma once

#include <atomic>
#include <iomanip>
#include <memory>
#include <random>
#include <set>
#include <sstream>

#include <openssl/sha.h>
#include <asio.hpp>

#include <nlohmann/json.hpp>

#include "Logger.hpp"

using ToMasterHandler = std::function<void(nlohmann::json)>;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    using MessageHandler = std::function<void(std::shared_ptr<WebSocketSession>, nlohmann::json)>;
    using CloseHandler = std::function<void(std::shared_ptr<WebSocketSession>)>;

    WebSocketSession(asio::ip::tcp::socket socket);
    uint64_t GetID();
    void start(MessageHandler msg_handler, CloseHandler close_handler);
    void send(const nlohmann::json& msg);
    void close();

private:
    asio::ip::tcp::socket socket_;
    asio::streambuf read_buf_;
    std::array<uint8_t, 14> header_buf_;
    bool handshake_done_ = false;
    MessageHandler msg_handler_;
    CloseHandler close_handler_;
    uint64_t id_;
    static std::atomic<uint64_t> next_id_;

    void doHandshake();
    void readFrame();
    void readPayload(uint64_t len, bool masked, uint8_t opcode);
    void processPayload(std::shared_ptr<std::vector<uint8_t>> payload, uint8_t opcode);
    void writeFrame(const std::vector<uint8_t>& frame);
    void handleTextMessage(const std::string& text);
    static std::string computeAcceptKey(const std::string& key);
};

class ChatWebSocketServer {
public:
    ChatWebSocketServer(asio::io_context& io, const std::string& address, unsigned short port, ToMasterHandler to_master);
    void start();
    void stop();
    void broadcast(const nlohmann::json& msg);

private:
    asio::io_context& io_;
    asio::ip::tcp::acceptor acceptor_;
    std::set<std::shared_ptr<WebSocketSession>> sessions_;
    std::mutex sessions_mutex_;
    bool stopped_ = false;
    ToMasterHandler to_master_;

    void doAccept();
    void onMessage(std::shared_ptr<WebSocketSession> session, nlohmann::json msg);
    void onClose(std::shared_ptr<WebSocketSession> session);

};
