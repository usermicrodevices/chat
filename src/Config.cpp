#include "Config.hpp"

Config Config::load(const std::string& path) {
    std::ifstream f(path);
    nlohmann::json j = nlohmann::json::parse(f);
    Config cfg;
    from_json(j["logging"], cfg.logging);
    from_json(j["serialization"], cfg.serialization);
    from_json(j["master"], cfg.master);
    from_json(j["worker"], cfg.worker);
    return cfg;
}

void Config::from_json(const nlohmann::json& j, Logging& l) {
    l.port = j.value("port", 8082);
    l.pattern = j.value("pattern", "[%Y-%m-%d %H:%M:%S.%e] [%P] [%^%l%$] %v");
    l.level = j.value("level", "trace");
    l.console = j.value("console", true);
    l.file = j.value("file", "logs/chat.log");
    l.max_files = j.value("max_files", 3);
    l.max_file_size = j.value("max_file_size", 10485760);
}

void Config::from_json(const nlohmann::json& j, Seriilization& s){
    s.db_path = j.value("db_path", "chat.db");
}

void Config::from_json(const nlohmann::json& j, Master& m) {
    m.worker_socket_path = j.value("worker_socket_path", "/tmp/chat_worker.sock");
    m.num_tasks = j.value("num_tasks", 4);
}

void Config::from_json(const nlohmann::json& j, Worker& w) {
    w.listen_address = j.value("listen_address", "0.0.0.0");
    w.listen_port = j.value("listen_port", 8080);
    w.num_io_threads = j.value("num_io_threads", 2);
    w.max_connections = j.value("max_connections", 1000);
}
