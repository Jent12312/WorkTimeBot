#include "Database.hpp"
#include <iostream>

Database::Database(const std::string& db_path) 
    : db(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) 
{
    initTables();
}

void Database::initTables() {
    // Таблица пользователей
    db.exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            base_rate INTEGER NOT NULL,
            has_bonus INTEGER NOT NULL
        )
    )");

    // Таблица смен
    db.exec(R"(
        CREATE TABLE IF NOT EXISTS shifts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            start_time INTEGER NOT NULL,
            end_time INTEGER DEFAULT 0,
            FOREIGN KEY(user_id) REFERENCES users(id)
        )
    )");
    // Таблица системных настроек
    db.exec(R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )");
}

bool Database::userExists(int64_t user_id) {
    SQLite::Statement query(db, "SELECT COUNT(*) FROM users WHERE id = ?");
    query.bind(1, user_id);
    query.executeStep();
    return query.getColumn(0).getInt() > 0;
}

void Database::addUser(int64_t user_id, const std::string& name, int base_rate, bool has_bonus) {
    SQLite::Statement query(db, "INSERT INTO users (id, name, base_rate, has_bonus) VALUES (?, ?, ?, ?)");
    query.bind(1, user_id);
    query.bind(2, name);
    query.bind(3, base_rate);
    query.bind(4, has_bonus ? 1 : 0);
    query.exec();
}

std::optional<User> Database::getUser(int64_t user_id) {
    SQLite::Statement query(db, "SELECT id, name, base_rate, has_bonus FROM users WHERE id = ?");
    query.bind(1, user_id);
    
    if (query.executeStep()) {
        return User{
            query.getColumn(0).getInt64(),
            query.getColumn(1).getString(),
            query.getColumn(2).getInt(),
            query.getColumn(3).getInt() != 0
        };
    }
    return std::nullopt;
}

bool Database::hasActiveShift(int64_t user_id) {
    SQLite::Statement query(db, "SELECT COUNT(*) FROM shifts WHERE user_id = ? AND end_time = 0");
    query.bind(1, user_id);
    query.executeStep();
    return query.getColumn(0).getInt() > 0;
}

void Database::startShift(int64_t user_id, int64_t start_timestamp) {
    SQLite::Statement query(db, "INSERT INTO shifts (user_id, start_time, end_time) VALUES (?, ?, 0)");
    query.bind(1, user_id);
    query.bind(2, start_timestamp);
    query.exec();
}

void Database::endShift(int64_t user_id, int64_t end_timestamp) {
    SQLite::Statement query(db, "UPDATE shifts SET end_time = ? WHERE user_id = ? AND end_time = 0");
    query.bind(1, end_timestamp);
    query.bind(2, user_id);
    query.exec();
}

std::optional<Shift> Database::getActiveShift(int64_t user_id) {
    SQLite::Statement query(db, "SELECT id, user_id, start_time, end_time FROM shifts WHERE user_id = ? AND end_time = 0");
    query.bind(1, user_id);
    
    if (query.executeStep()) {
        return Shift{
            query.getColumn(0).getInt(),
            query.getColumn(1).getInt64(),
            query.getColumn(2).getInt64(),
            query.getColumn(3).getInt64()
        };
    }
    return std::nullopt;
}

std::vector<User> Database::getAllUsers() {
    std::vector<User> result;
    SQLite::Statement query(db, "SELECT id, name, base_rate, has_bonus FROM users");
    while (query.executeStep()) {
        result.push_back(User{
            query.getColumn(0).getInt64(),
            query.getColumn(1).getString(),
            query.getColumn(2).getInt(),
            query.getColumn(3).getInt() != 0
        });
    }
    return result;
}

std::string Database::getSetting(const std::string& key) {
    SQLite::Statement query(db, "SELECT value FROM settings WHERE key = ?");
    query.bind(1, key);
    if (query.executeStep()) {
        return query.getColumn(0).getString();
    }
    return "";
}

void Database::setSetting(const std::string& key, const std::string& value) {
    SQLite::Statement query(db, "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    query.bind(1, key);
    query.bind(2, value);
    query.exec();
}

std::vector<Shift> Database::getShiftsInRange(int64_t user_id, int64_t start_time, int64_t end_time) {
    std::vector<Shift> result;
    SQLite::Statement query(db,
        "SELECT id, user_id, start_time, end_time FROM shifts "
        "WHERE user_id = ? AND end_time >= ? AND end_time <= ? "
        "ORDER BY start_time ASC"
    );
    query.bind(1, user_id);
    query.bind(2, start_time);
    query.bind(3, end_time);

    while (query.executeStep()) {
        result.push_back(Shift{
            query.getColumn(0).getInt(),
            query.getColumn(1).getInt64(),
            query.getColumn(2).getInt64(),
            query.getColumn(3).getInt64()
        });
    }
    return result;
}

void Database::updateUserName(int64_t user_id, const std::string& name) {
    SQLite::Statement query(db, "UPDATE users SET name = ? WHERE id = ?");
    query.bind(1, name);
    query.bind(2, user_id);
    query.exec();
}

void Database::updateUserRate(int64_t user_id, int rate) {
    SQLite::Statement query(db, "UPDATE users SET base_rate = ? WHERE id = ?");
    query.bind(1, rate);
    query.bind(2, user_id);
    query.exec();
}

void Database::updateUserBonus(int64_t user_id, bool has_bonus) {
    SQLite::Statement query(db, "UPDATE users SET has_bonus = ? WHERE id = ?");
    query.bind(1, has_bonus ? 1 : 0);
    query.bind(2, user_id);
    query.exec();
}