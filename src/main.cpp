#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include "Database.hpp"
#include "BotCore.hpp"

int main() {
    const char* token_env = std::getenv("BOT_TOKEN");
    if (!token_env) {
        fmt::print(stderr, "❌ Критическая ошибка: Переменная окружения BOT_TOKEN не задана!\n");
        return 1;
    }
    std::string token(token_env);

    std::string db_dir = "./data";
    if (!std::filesystem::exists(db_dir)) {
        std::filesystem::create_directory(db_dir);
    }
    std::string db_path = db_dir + "/worktime.db";

    try {
        fmt::print("🚀 Инициализация базы данных: {}\n", db_path);
        Database db(db_path);
        
        BotCore botCore(token, db);
        botCore.run();

    } catch (const std::exception& e) {
        fmt::print(stderr, "❌ Критическая ошибка при работе приложения: {}\n", e.what());
        return 1;
    }

    return 0;
}
