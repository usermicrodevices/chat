#pragma once

#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

struct Logging {
    uint32_t port = 8082;
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%P] [%^%l%$] %v";
    std::string level = "debug";
    bool console = true;
    std::string file = "logs/chat.log";
    uint32_t max_files = 3;
    uint64_t max_file_size = 10485760;
};

struct Seriilization {std::string db_path = "chat.db";};

struct Master {
    std::string worker_socket_path = "/tmp/chat_worker.sock";
    unsigned int num_tasks = 3;//count child forks
};

struct Worker {
    std::string listen_address = "0.0.0.0";
    unsigned short listen_port = 8080;
    unsigned int num_io_threads = 2;
    unsigned int max_connections = 100;
};

class Config {
public:
    Logging logging;
    Seriilization serialization;
    Master master;
    Worker worker;
    static Config load(const std::string& path);

private:
    static void from_json(const nlohmann::json& j, Logging& l);
    static void from_json(const nlohmann::json& j, Seriilization& s);
    static void from_json(const nlohmann::json& j, Master& m);
    static void from_json(const nlohmann::json& j, Worker& w);
};
