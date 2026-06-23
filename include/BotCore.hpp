#pragma once

#include <tgbot/tgbot.h>
#include "Database.hpp"
#include <map>
#include <string>
#include <thread>

enum class RegState {
    NONE,
    WAITING_NAME,
    WAITING_RATE,
    WAITING_BONUS,
    WAITING_NAME_CHANGE
};

struct TempUserData {
    std::string name;
    int rate = 0;
};

class BotCore {
public:
    BotCore(const std::string& token, Database& db);
    void run();

private:
    TgBot::Bot bot;
    Database& db;
    
    std::map<int64_t, RegState> userStates;
    std::map<int64_t, TempUserData> tempUsers;

    void setupHandlers();
    void setupCallbackHandlers(); // <-- Новый метод для Inline-кнопок
    void handleMessage(TgBot::Message::Ptr message);
    
    // Клавиатуры Reply (нижние)
    TgBot::ReplyKeyboardMarkup::Ptr getRateKeyboard();
    TgBot::ReplyKeyboardMarkup::Ptr getBonusKeyboard();
    TgBot::ReplyKeyboardMarkup::Ptr getMainMenuKeyboard();

    // Клавиатура Inline (прикрепленная) для выбора времени
    TgBot::InlineKeyboardMarkup::Ptr getTimeChoiceKeyboard(const std::string& prefix);
    TgBot::InlineKeyboardMarkup::Ptr getHistoryMenuKeyboard();
    TgBot::InlineKeyboardMarkup::Ptr getSettingsKeyboard(bool has_bonus);
    TgBot::InlineKeyboardMarkup::Ptr getSettingsRateKeyboard();
    
    // Вспомогательный метод для красивого вывода времени (HH:MM)
    std::string formatTime(int64_t timestamp);
    // Метод планировщика (будет крутиться в отдельном потоке)
    void weeklyReportScheduler();
    
    // Метод генерации и отправки отчетов всем пользователям
    void sendWeeklyReports(int64_t prev_week_start);
};