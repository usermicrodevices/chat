#include "WorkerClient.hpp"

WorkerClient::WorkerClient(const Config& cfg, int child_fd)
    : cfg_(cfg), child_fd_(child_fd) {}

void WorkerClient::run() {
    Logger::Trace("Worker child started (PID {})", getpid());
    signal(SIGTERM, SIG_IGN);
    setupIpc();
    ws_server_ = std::make_unique<ChatWebSocketServer>(io_,
    cfg_.worker.listen_address,
    cfg_.worker.listen_port,
    [this](nlohmann::json msg) {
        if (ipc_) {
            IpcMessage out{0x10, {}};
            std::string str = msg.dump();
            out.payload.assign(str.begin(), str.end());
            ipc_->asyncSend(out);
        }
    });
    ws_server_->start();
    Logger::Trace("Worker {} entering io_context run", getpid());
    io_.run();
    Logger::Trace("Worker {} io_context finished, exiting", getpid());
}

void WorkerClient::setupIpc() {
    ipc_ = std::make_shared<IpcConnection>(io_, child_fd_);
    ipc_->start(
    [this](IpcMessage msg) { onIpcMessage(msg); },
    [this](std::error_code ec) {
        if (ec) {
            Logger::Trace("Worker IPC closed, shutting down.");
            io_.stop();
        }
    });
}

void WorkerClient::onIpcMessage(IpcMessage msg) {
    if (msg.cmd == 0x11) {
        std::string json_str(msg.payload.begin(), msg.payload.end());
        try {
            nlohmann::json j = nlohmann::json::parse(json_str);
            ws_server_->broadcast(j);
        } catch (const std::exception& e) {
            Logger::Error("Bad broadcast: {}", e.what());
        }
    }
}
