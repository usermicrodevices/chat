#include <iostream>

#include "Config.hpp"
#include "Logger.hpp"
#include "WorkerMaster.hpp"

int main(int argc, char** argv) {
    std::string args_str;
    for (int i = 0; i < argc; ++i) {
        if (i > 0) args_str += " ";
        args_str += argv[i];
    }
    try {
        Config cfg = Config::load("config.json");
        Logger::Initialize({
            {"port", cfg.logging.port},
            {"file", cfg.logging.file},
            {"max_file_size", cfg.logging.max_file_size},
            {"max_files", cfg.logging.max_files},
            {"console", cfg.logging.console},
            {"level", cfg.logging.level},
            {"pattern", cfg.logging.pattern}
        });
        Logger::Trace("Start server: {}", args_str);
        asio::io_context master_io;
        auto master = std::make_shared<WorkerMaster>(master_io, cfg);
        master->run();
        Logger::Trace("Server shut down gracefully.");
    } catch (const std::exception& e) {
        Logger::InitializeDefaults();
        Logger::Error("Fatal: {}", e.what());
        return 1;
    }
    return 0;
}
