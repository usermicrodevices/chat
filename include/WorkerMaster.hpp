#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <asio.hpp>

#include "Config.hpp"
#include "Logger.hpp"
#include "Serializer.hpp"
#include "Ipc.hpp"
#include "WorkerClient.hpp"

class WorkerMaster : public std::enable_shared_from_this<WorkerMaster> {
public:
    WorkerMaster(asio::io_context& io, const Config& cfg);
    ~WorkerMaster();
    void run();
    void stop();

private:
    asio::io_context& io_;
    Config cfg_;
    std::vector<std::shared_ptr<IpcConnection>> child_connections_;
    std::vector<pid_t> child_pids_;
    std::unique_ptr<Serializer> store_;
    std::queue<ChatMessage> sql_queue_;
    std::mutex sql_mutex_;
    std::condition_variable sql_cv_;
    std::thread sql_thread_;
    std::atomic<bool> running_{true};
    asio::signal_set signals_;

    void spawnWorkers();
    void onChildMessage(int child_id, IpcMessage msg);
    void broadcastToChildren(IpcMessage msg);
    void sqliteThread();
    void stopAllChildren();
};
