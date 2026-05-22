#include "WorkerMaster.hpp"

WorkerMaster::WorkerMaster(asio::io_context& io, const Config& cfg)
    : io_(io), cfg_(cfg), signals_(io, SIGINT, SIGTERM) {
    store_ = std::make_unique<Serializer>(cfg_.serialization.db_path);
    store_->initSchema();
    signals_.async_wait([this](std::error_code, int) {
        Logger::Trace("Master received shutdown signal");
        stop();
    });
    spawnWorkers();
    sql_thread_ = std::thread(&WorkerMaster::sqliteThread, this);
    Logger::Trace("Master started with {} workers", child_connections_.size());
}

WorkerMaster::~WorkerMaster() {
    stop();
}

void WorkerMaster::spawnWorkers() {
    for (unsigned int i = 0; i < cfg_.master.num_tasks; ++i) {
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) != 0) {
            Logger::Error("socketpair failed for worker {}", i);
            throw std::runtime_error("socketpair failed");
        }
        pid_t pid = fork();
        if (pid == 0) {
            close(pair[0]);
            WorkerClient child(cfg_, pair[1]);
            child.run();
            _exit(0);
        } else if (pid > 0) {
            close(pair[1]);
            child_pids_.push_back(pid);
            auto conn = std::make_shared<IpcConnection>(io_, pair[0]);
            child_connections_.push_back(conn);
            conn->start(
                [this, i](IpcMessage msg) { onChildMessage(i, msg); },
                [this, i](std::error_code ec) {
                    Logger::Warn("Child {} IPC error: {}", i, ec.message());
                });
            Logger::Debug("Spawned worker {} (PID {})", i, pid);
        } else {
            Logger::Error("fork failed for worker {}", i);
            throw std::runtime_error("fork failed");
        }
    }
}

void WorkerMaster::onChildMessage(int child_id, IpcMessage msg) {
    if (msg.cmd == 0x10) {
        std::string json_str(msg.payload.begin(), msg.payload.end());
        try {
            nlohmann::json j = nlohmann::json::parse(json_str);
            ChatMessage chat;
            chat.sender = j.value("sender", "unknown");
            chat.content = j.value("content", "");
            chat.room = j.value("room", "general");
            chat.timestamp = std::time(nullptr);
            {
                std::lock_guard<std::mutex> lock(sql_mutex_);
                sql_queue_.push(chat);
            }
            sql_cv_.notify_one();
            nlohmann::json broadcast_j = {
                {"type", "chat"},
                {"sender", chat.sender},
                {"content", chat.content},
                {"room", chat.room}
            };
            IpcMessage out{0x11, {}};
            std::string broadcast_str = broadcast_j.dump();
            out.payload.assign(broadcast_str.begin(), broadcast_str.end());
            broadcastToChildren(out);
        } catch (const std::exception& err) {
            Logger::Error("Bad message from child {}: {}", child_id, err.what());
        }
    }
}

void WorkerMaster::broadcastToChildren(IpcMessage msg) {
    for (auto& conn : child_connections_) {
        if (conn) conn->send(msg);
    }
}

void WorkerMaster::stop() {
    Logger::Trace("Master stopping...");
    if (!running_.exchange(false)) return;
    Logger::Trace("Closing IPC connections...");
    for (auto& conn : child_connections_) {
        if (conn) conn->close();
    }
    Logger::Trace("Notifying SQL thread...");
    sql_cv_.notify_one();
    if (sql_thread_.joinable()) {
        Logger::Trace("Joining SQL thread...");
        sql_thread_.join();
        Logger::Trace("SQL thread joined.");
    }
    Logger::Trace("Terminating child processes...");
    for (pid_t pid : child_pids_) {
        Logger::Trace("Sending SIGTERM to PID {}", pid);
        kill(pid, SIGTERM);
    }
    for (pid_t pid : child_pids_) {
        Logger::Trace("Waiting for PID {}", pid);
        int status;
        pid_t ret = waitpid(pid, &status, 0);
        if (ret == -1) {
            Logger::Error("waitpid failed for {}: {}", pid, strerror(errno));
        } else {
            Logger::Trace("PID {} exited with status {}", pid, status);
        }
    }
    Logger::Trace("Stopping signals...");
    signals_.cancel();
    Logger::Trace("Stopping io_context...");
    io_.stop();
    Logger::Trace("Master stop() finished.");
}

void WorkerMaster::run() {
    io_.run();
}

void WorkerMaster::sqliteThread() {
    while (running_ || !sql_queue_.empty()) {
        std::unique_lock<std::mutex> lock(sql_mutex_);
        sql_cv_.wait(lock, [this] { return !sql_queue_.empty() || !running_; });
        if (!running_ && sql_queue_.empty()) break;
        ChatMessage msg = sql_queue_.front();
        sql_queue_.pop();
        lock.unlock();
        store_->storeMessage(msg);
    }
}
