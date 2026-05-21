#pragma once

#include <memory>
#include <unistd.h>

#include <asio.hpp>

#include "Config.hpp"
#include "Ipc.hpp"
#include "WebSocketSession.hpp"

class WorkerClient {
public:
    WorkerClient(const Config& cfg, int child_fd);
    void run();

private:
    Config cfg_;
    asio::io_context io_;
    int child_fd_;
    std::shared_ptr<IpcConnection> ipc_;
    std::unique_ptr<ChatWebSocketServer> ws_server_;
    std::thread io_thread_;

    void setupIpc();
    void onIpcMessage(IpcMessage msg);
    void onWebSocketMessage(std::shared_ptr<WebSocketSession> session, nlohmann::json msg);
};
