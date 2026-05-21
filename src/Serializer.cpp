#include "Serializer.hpp"
Serializer::Serializer(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        Logger::Error("Failed to open DB: {}", std::string(sqlite3_errmsg(db_)));
        throw std::runtime_error("DB open failed");
    }
}
Serializer::~Serializer() {
    if (db_) sqlite3_close(db_);
}
bool Serializer::initSchema() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender TEXT NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            room TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_room_time ON messages(room, timestamp);
    )";
    return execSql(sql);
}
bool Serializer::storeMessage(const ChatMessage& msg) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO messages (sender, content, timestamp, room) VALUES (?,?,?,?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, msg.sender.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, msg.content.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, msg.timestamp);
    sqlite3_bind_text(stmt, 4, msg.room.c_str(), -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}
std::vector<ChatMessage> Serializer::getRecentMessages(const std::string& room, int limit) {
    std::vector<ChatMessage> res;
    sqlite3_stmt* stmt;
    std::string sql = "SELECT id, sender, content, timestamp FROM messages WHERE room = ? ORDER BY timestamp DESC LIMIT ?";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return res;
    sqlite3_bind_text(stmt, 1, room.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChatMessage m;
        m.id = sqlite3_column_int64(stmt, 0);
        m.sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        m.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        m.timestamp = sqlite3_column_int64(stmt, 3);
        m.room = room;
        res.push_back(m);
    }
    sqlite3_finalize(stmt);
    return res;
}
bool Serializer::execSql(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        Logger::Error("SQL error: {}", std::string(err));
        sqlite3_free(err);
        return false;
    }
    return true;
}
