#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <string>
#include <vector>
#include <optional>

// Структура для хранения данных пользователя
struct User {
    int64_t id;
    std::string name;
    int base_rate;     // Ставка (200, 225, 250, 280)
    bool has_bonus;    // Есть ли надбавка 25 руб
};

// Структура для хранения данных о смене
struct Shift {
    int id;
    int64_t user_id;
    int64_t start_time; // Unix timestamp
    int64_t end_time;   // Unix timestamp (0, если смена еще идет)
};

class Database {
public:
    // Конструктор открывает или создает БД
    explicit Database(const std::string& db_path);

    // --- Методы для пользователей ---
    bool userExists(int64_t user_id);
    void addUser(int64_t user_id, const std::string& name, int base_rate, bool has_bonus);
    std::optional<User> getUser(int64_t user_id);
    void updateUserName(int64_t user_id, const std::string& name);
    void updateUserRate(int64_t user_id, int rate);
    void updateUserBonus(int64_t user_id, bool has_bonus);

    // --- Методы для смен ---
    bool hasActiveShift(int64_t user_id);
    void startShift(int64_t user_id, int64_t start_timestamp);
    void endShift(int64_t user_id, int64_t end_timestamp);
    
    // Получить активную смену пользователя (чтобы узнать время старта)
    std::optional<Shift> getActiveShift(int64_t user_id);
    // Получить список всех зарегистрированных сотрудников
    std::vector<User> getAllUsers();

    // Получить все завершенные смены пользователя за период времени
    std::vector<Shift> getShiftsInRange(int64_t user_id, int64_t start_time, int64_t end_time);

    // Системные настройки (для сохранения даты рассылки)
    std::string getSetting(const std::string& key);
    void setSetting(const std::string& key, const std::string& value);

private:
    SQLite::Database db;

    // Метод для создания таблиц, если их нет
    void initTables();
};