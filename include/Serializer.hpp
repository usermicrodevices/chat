#pragma once

#include <functional>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "Logger.hpp"

struct ChatMessage {
    int64_t id;
    std::string sender;
    std::string content;
    int64_t timestamp;
    std::string room;
};

class Serializer {
public:
    explicit Serializer(const std::string& db_path);
    ~Serializer();
    bool initSchema();
    bool storeMessage(const ChatMessage& msg);
    std::vector<ChatMessage> getRecentMessages(const std::string& room, int limit = 100);

private:
    sqlite3* db_ = nullptr;
    bool execSql(const std::string& sql);
};
