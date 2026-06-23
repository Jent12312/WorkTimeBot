#include "BotCore.hpp"
#include "TimeUtils.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

BotCore::BotCore(const std::string& token, Database& db) 
    : bot(token), db(db) 
{
    try {
        fmt::print("🔄 Сброс старых настроек Telegram...\n");
        bot.getApi().deleteWebhook(true); 
    } catch (const std::exception& e) {
        fmt::print("Предупреждение при очистке вебхука: {}\n", e.what());
    }

    // Запускаем единственный метод настройки обработчиков
    setupHandlers();
}

std::string BotCore::formatTime(int64_t timestamp) {
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm* tm_info = std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(tm_info, "%H:%M");
    return ss.str();
}

TgBot::InlineKeyboardMarkup::Ptr BotCore::getTimeChoiceKeyboard(const std::string& prefix) {
    auto kbd = std::make_shared<TgBot::InlineKeyboardMarkup>();
    
    std::time_t now = std::time(nullptr);
    std::time_t prev_30 = (now / 1800) * 1800;
    std::time_t next_30 = prev_30 + 1800;

    auto row = std::vector<TgBot::InlineKeyboardButton::Ptr>();
    
    auto btn1 = std::make_shared<TgBot::InlineKeyboardButton>();
    btn1->text = formatTime(prev_30);
    btn1->callbackData = prefix + "_" + std::to_string(prev_30);
    
    auto btn2 = std::make_shared<TgBot::InlineKeyboardButton>();
    btn2->text = formatTime(next_30);
    btn2->callbackData = prefix + "_" + std::to_string(next_30);

    row.push_back(btn1);
    row.push_back(btn2);
    kbd->inlineKeyboard.push_back(row);
    
    return kbd;
}

void BotCore::setupHandlers() {
    // --- 1. ОБРАБОТКА КОМАНДЫ /start ---
    bot.getEvents().onCommand("start", [this](TgBot::Message::Ptr message) {
        int64_t userId = message->from->id;
        fmt::print("ℹ️ Получена команда /start от {}\n", userId);
        
        if (db.userExists(userId)) {
            bot.getApi().sendMessage(userId, "С возвращением! Выбери действие:", nullptr, nullptr, getMainMenuKeyboard());
            userStates[userId] = RegState::NONE;
        } else {
            bot.getApi().sendMessage(userId, "Привет! Давай настроим твой профиль.\nВведи свое Имя:");
            userStates[userId] = RegState::WAITING_NAME;
        }
    });

    // --- 2. ОБРАБОТКА ВСЕХ ТЕКСТОВЫХ СООБЩЕНИЙ ---
    bot.getEvents().onAnyMessage([this](TgBot::Message::Ptr message) {
        if (message->text.starts_with("/")) return; // Игнорируем команды
        
        fmt::print("📩 Получено сообщение: \"{}\" от {}\n", message->text, message->from->id);
        handleMessage(message);
    });

    // --- 3. ОБРАБОТКА НАЖАТИЙ НА INLINE-КНОПКИ (CALLBACK) ---
    bot.getEvents().onCallbackQuery([this](TgBot::CallbackQuery::Ptr query) {
        fmt::print("👉 Нажата кнопка! Получены данные: {}\n", query->data);

        try {
            bot.getApi().answerCallbackQuery(query->id);

            int64_t userId = query->from->id;
            std::string data = query->data;

            if (!query->message) {
                fmt::print(stderr, "Ошибка: Сообщение кнопки утеряно.\n");
                return;
            }

            if (data.starts_with("start_")) {
                int64_t timestamp = std::stoll(data.substr(6));
                
                if (db.hasActiveShift(userId)) {
                    bot.getApi().sendMessage(userId, "⚠️ У тебя уже есть активная смена!");
                    return;
                }

                db.startShift(userId, timestamp);
                fmt::print("💾 Смена успешно записана в БД для пользователя {}\n", userId);
                
                bot.getApi().editMessageText(
                    "✅ Смена успешно начата в " + formatTime(timestamp),
                    query->message->chat->id, query->message->messageId
                );
            } 
            else if (data.starts_with("end_")) {
                int64_t end_timestamp = std::stoll(data.substr(4));
                
                auto shift_opt = db.getActiveShift(userId);
                if (!shift_opt) {
                    bot.getApi().sendMessage(userId, "⚠️ У тебя нет активной смены!");
                    return;
                }

                int64_t start_timestamp = shift_opt->start_time;

                if (end_timestamp <= start_timestamp) {
                    bot.getApi().sendMessage(userId, "❌ Ошибка: Время окончания не может быть раньше или равно времени начала!");
                    return;
                }

                db.endShift(userId, end_timestamp);
                fmt::print("💾 Смена закрыта в БД для пользователя {}\n", userId);

                auto user_opt = db.getUser(userId);
                if (!user_opt) {
                    bot.getApi().sendMessage(userId, "❌ Ошибка профиля. Нажми /start");
                    return;
                }
                auto user = user_opt.value();
                
                double hours_worked = (end_timestamp - start_timestamp) / 3600.0;
                double earned_base = hours_worked * user.base_rate;
                double earned_bonus = user.has_bonus ? (hours_worked * 25) : 0.0;
                double total_earned = earned_base + earned_bonus;

                int hours = static_cast<int>(hours_worked);
                int minutes = static_cast<int>((hours_worked - hours) * 60);

                std::string report = fmt::format(
                    "🛑 <b>Смена окончена!</b>\n"
                    "Начало: {}\n"
                    "Конец: {}\n"
                    "Отработано: {} ч. {} мин.\n\n"
                    "💰 <b>Расчет:</b>\n"
                    "По ставке ({} руб/ч): {:.2f} руб.\n",
                    formatTime(start_timestamp), formatTime(end_timestamp),
                    hours, minutes, user.base_rate, earned_base
                );

                if (user.has_bonus) {
                    report += fmt::format("Надбавка (25 руб/ч): {:.2f} руб.\n", earned_bonus);
                }

                report += fmt::format("──────────────\n💵 <b>ИТОГО ЗА СМЕНУ: {:.2f} руб.</b>", total_earned);

                bot.getApi().editMessageText(
                    report, query->message->chat->id, query->message->messageId,
                    "", "HTML"
                );
            }
            else if (data.starts_with("hist_")) {
                std::time_t now = std::time(nullptr);
                std::time_t start_time = 0;
                std::time_t end_time = now;
                std::string period_name;

                if (data == "hist_curr_week") {
                    start_time = TimeUtils::getStartOfWeek(now);
                    period_name = "Эта неделя";
                } 
                else if (data == "hist_prev_week") {
                    std::time_t start_this_week = TimeUtils::getStartOfWeek(now);
                    start_time = start_this_week - (7 * 24 * 3600);
                    end_time = start_this_week - 1;
                    period_name = "Прошлая неделя";
                } 
                else if (data == "hist_curr_month") {
                    start_time = TimeUtils::getStartOfMonth(now);
                    period_name = "Этот месяц";
                }

                auto shifts = db.getShiftsInRange(userId, start_time, end_time);
                auto user = db.getUser(userId).value();
                auto report = TimeUtils::calculateReport(shifts, user);

                std::string text_report = fmt::format(
                    "🗄 <b>Отчет за период: {}</b>\n"
                    "───────────────────\n"
                    "👤 Сотрудник: <b>{}</b>\n"
                    "📅 Отработано смен: <b>{}</b>\n"
                    "⏱ Всего часов: <b>{:.2f} ч.</b>\n\n"
                    "💳 <b>Заработок:</b>\n"
                    "• Оклад ({} руб/ч): {:.2f} руб.\n",
                    period_name, user.name, report.shift_count, report.total_hours, user.base_rate, report.base_earnings
                );

                if (user.has_bonus) {
                    text_report += fmt::format("• Надбавка (25 руб/ч): {:.2f} руб.\n", report.bonus_earnings);
                }

                if (report.premium > 0) {
                    text_report += fmt::format("• 🎁 Премия (ПТ+СБ отработаны): <b>{:.2f} руб.</b>\n", report.premium);
                } else if (report.shift_count >= 5) {
                    text_report += "• 🎁 Премия: <i>нет (не отработаны ПТ или СБ)</i>\n";
                }

                text_report += fmt::format("───────────────────\n💵 <b>ИТОГО ЗАРАБОТАНО: {:.2f} руб.</b>", report.grand_total);

                bot.getApi().sendMessage(userId, text_report, nullptr, nullptr, nullptr, "HTML");
            }
            else if (data.starts_with("sett_")) {
                if (data == "sett_edit_name") {
                    userStates[userId] = RegState::WAITING_NAME_CHANGE;
                    bot.getApi().sendMessage(userId, "👤 Введи новое Имя:");
                } 
                else if (data == "sett_edit_rate") {
                    bot.getApi().editMessageText(
                        "💳 Выбери новую часовую ставку:",
                        query->message->chat->id, query->message->messageId,
                        "", "", nullptr, getSettingsRateKeyboard()
                    );
                } 
                else if (data.starts_with("sett_rate_val_")) {
                    int new_rate = std::stoi(data.substr(14));
                    db.updateUserRate(userId, new_rate);
                    
                    auto user = db.getUser(userId).value();
                    bot.getApi().editMessageText(
                        fmt::format("⚙️ <b>Профиль обновлен!</b>\n\n👤 Имя: {}\n💳 Ставка: {} руб/ч\n🎁 Надбавка (25р): {}", 
                                    user.name, user.base_rate, user.has_bonus ? "Включена" : "Отключена"),
                        query->message->chat->id, query->message->messageId,
                        "", "HTML", nullptr, getSettingsKeyboard(user.has_bonus)
                    );
                } 
                else if (data == "sett_toggle_bonus") {
                    auto user = db.getUser(userId).value();
                    db.updateUserBonus(userId, !user.has_bonus);
                    
                    auto updated_user = db.getUser(userId).value();
                    bot.getApi().editMessageText(
                        fmt::format("⚙️ <b>Профиль обновлен!</b>\n\n👤 Имя: {}\n💳 Ставка: {} руб/ч\n🎁 Надбавка (25р): {}", 
                                    updated_user.name, updated_user.base_rate, updated_user.has_bonus ? "Включена" : "Отключена"),
                        query->message->chat->id, query->message->messageId,
                        "", "HTML", nullptr, getSettingsKeyboard(updated_user.has_bonus)
                    );
                }
            }
        } 
        catch (const TgBot::TgException& e) {
            fmt::print(stderr, "❌ Telegram API Ошибка: {}\n", e.what());
        } 
        catch (const std::exception& e) {
            fmt::print(stderr, "❌ Внутренняя Ошибка C++: {}\n", e.what());
        }
    });
}

void BotCore::handleMessage(TgBot::Message::Ptr message) {
    int64_t userId = message->from->id;
    std::string text = message->text;

    if (userStates.find(userId) == userStates.end()) {
        if (!db.userExists(userId)) {
            userStates[userId] = RegState::WAITING_NAME;
            bot.getApi().sendMessage(userId, "Давай настроим твой профиль.\nВведи свое Имя:");
            return;
        } else {
            userStates[userId] = RegState::NONE;
        }
    }

    RegState state = userStates[userId];

    switch (state) {
        case RegState::WAITING_NAME: {
            tempUsers[userId].name = text;
            userStates[userId] = RegState::WAITING_RATE;
            bot.getApi().sendMessage(userId, "Отлично, " + text + "! Выбери свою ставку (руб/час):", nullptr, nullptr, getRateKeyboard());
            break;
        }
        case RegState::WAITING_RATE: {
            if (text == "200" || text == "225" || text == "250" || text == "280") {
                tempUsers[userId].rate = std::stoi(text);
                userStates[userId] = RegState::WAITING_BONUS;
                bot.getApi().sendMessage(userId, "Есть ли у тебя фиксированная надбавка (25 руб/час)?", nullptr, nullptr, getBonusKeyboard());
            } else {
                bot.getApi().sendMessage(userId, "Пожалуйста, выбери ставку, используя кнопки ниже:", nullptr, nullptr, getRateKeyboard());
            }
            break;
        }
        case RegState::WAITING_BONUS: {
            bool hasBonus = (text == "Да");
            if (text != "Да" && text != "Нет") {
                bot.getApi().sendMessage(userId, "Пожалуйста, ответь «Да» или «Нет» кнопками ниже:", nullptr, nullptr, getBonusKeyboard());
                return;
            }
            db.addUser(userId, tempUsers[userId].name, tempUsers[userId].rate, hasBonus);
            userStates[userId] = RegState::NONE;
            tempUsers.erase(userId);
            bot.getApi().sendMessage(userId, "✅ Регистрация завершена!", nullptr, nullptr, getMainMenuKeyboard());
            break;
        }
        case RegState::WAITING_NAME_CHANGE: {
            db.updateUserName(userId, text);
            userStates[userId] = RegState::NONE;
            bot.getApi().sendMessage(userId, "👤 Имя успешно изменено на " + text + "!", nullptr, nullptr, getMainMenuKeyboard());
            break;
        }
        case RegState::NONE: {
            if (text == "▶️ Начать смену") {
                if (db.hasActiveShift(userId)) {
                    bot.getApi().sendMessage(userId, "⚠️ У тебя уже есть незавершенная смена!");
                } else {
                    bot.getApi().sendMessage(userId, "Выбери время начала смены (округлено до 30 мин):", nullptr, nullptr, getTimeChoiceKeyboard("start"));
                }
            } else if (text == "⏹ Закончить смену") {
                if (!db.hasActiveShift(userId)) {
                    bot.getApi().sendMessage(userId, "⚠️ Ты еще не начинал смену!");
                } else {
                    bot.getApi().sendMessage(userId, "Выбери время окончания смены:", nullptr, nullptr, getTimeChoiceKeyboard("end"));
                }
            } else if (text == "🗄 История и Отчеты") {
                bot.getApi().sendMessage(userId, "Выбери интересующий период отчетов:", nullptr, nullptr, getHistoryMenuKeyboard());
            } else if (text == "⚙️ Настройки") {
                auto user = db.getUser(userId).value();
                std::string msg = fmt::format(
                    "⚙️ <b>Твои текущие настройки:</b>\n\n👤 Имя: <b>{}</b>\n💳 Ставка: <b>{} руб/ч</b>\n🎁 Надбавка (25р): <b>{}</b>\n\nВыбери, что нужно изменить:",
                    user.name, user.base_rate, user.has_bonus ? "Включена" : "Отключена"
                );
                bot.getApi().sendMessage(userId, msg, nullptr, nullptr, getSettingsKeyboard(user.has_bonus), "HTML");
            } else {
                bot.getApi().sendMessage(userId, "Неизвестная команда.", nullptr, nullptr, getMainMenuKeyboard());
            }
            break;
        }
    }
}

TgBot::ReplyKeyboardMarkup::Ptr BotCore::getRateKeyboard() {
    auto kbd = std::make_shared<TgBot::ReplyKeyboardMarkup>();
    kbd->resizeKeyboard = true; kbd->oneTimeKeyboard = true;
    kbd->keyboard.push_back({std::make_shared<TgBot::KeyboardButton>("200"), std::make_shared<TgBot::KeyboardButton>("225")});
    kbd->keyboard.push_back({std::make_shared<TgBot::KeyboardButton>("250"), std::make_shared<TgBot::KeyboardButton>("280")});
    return kbd;
}

TgBot::ReplyKeyboardMarkup::Ptr BotCore::getBonusKeyboard() {
    auto kbd = std::make_shared<TgBot::ReplyKeyboardMarkup>();
    kbd->resizeKeyboard = true; kbd->oneTimeKeyboard = true;
    kbd->keyboard.push_back({std::make_shared<TgBot::KeyboardButton>("Да"), std::make_shared<TgBot::KeyboardButton>("Нет")});
    return kbd;
}

TgBot::ReplyKeyboardMarkup::Ptr BotCore::getMainMenuKeyboard() {
    auto kbd = std::make_shared<TgBot::ReplyKeyboardMarkup>();
    kbd->resizeKeyboard = true;
    kbd->keyboard.push_back({std::make_shared<TgBot::KeyboardButton>("▶️ Начать смену")});
    kbd->keyboard.push_back({std::make_shared<TgBot::KeyboardButton>("⏹ Закончить смену")});
    kbd->keyboard.push_back({std::make_shared<TgBot::KeyboardButton>("🗄 История и Отчеты"), std::make_shared<TgBot::KeyboardButton>("⚙️ Настройки")});
    return kbd;
}

TgBot::InlineKeyboardMarkup::Ptr BotCore::getSettingsKeyboard(bool has_bonus) {
    auto kbd = std::make_shared<TgBot::InlineKeyboardMarkup>();
    
    auto row1 = std::vector<TgBot::InlineKeyboardButton::Ptr>();
    auto btn1 = std::make_shared<TgBot::InlineKeyboardButton>();
    btn1->text = "👤 Изменить имя";
    btn1->callbackData = "sett_edit_name";
    row1.push_back(btn1);

    auto row2 = std::vector<TgBot::InlineKeyboardButton::Ptr>();
    auto btn2 = std::make_shared<TgBot::InlineKeyboardButton>();
    btn2->text = "💳 Изменить ставку";
    btn2->callbackData = "sett_edit_rate";
    row2.push_back(btn2);

    auto row3 = std::vector<TgBot::InlineKeyboardButton::Ptr>();
    auto btn3 = std::make_shared<TgBot::InlineKeyboardButton>();
    btn3->text = has_bonus ? "🎁 Отключить надбавку (25р)" : "🎁 Включить надбавку (25р)";
    btn3->callbackData = "sett_toggle_bonus";
    row3.push_back(btn3);

    kbd->inlineKeyboard.push_back(row1);
    kbd->inlineKeyboard.push_back(row2);
    kbd->inlineKeyboard.push_back(row3);
    return kbd;
}

TgBot::InlineKeyboardMarkup::Ptr BotCore::getSettingsRateKeyboard() {
    auto kbd = std::make_shared<TgBot::InlineKeyboardMarkup>();
    auto row = std::vector<TgBot::InlineKeyboardButton::Ptr>();

    std::vector<std::string> rates = {"200", "225", "250", "280"};
    for (const auto& r : rates) {
        auto btn = std::make_shared<TgBot::InlineKeyboardButton>();
        btn->text = r;
        btn->callbackData = "sett_rate_val_" + r;
        row.push_back(btn);
    }
    kbd->inlineKeyboard.push_back(row);
    return kbd;
}

TgBot::InlineKeyboardMarkup::Ptr BotCore::getHistoryMenuKeyboard() {
    auto kbd = std::make_shared<TgBot::InlineKeyboardMarkup>();
    auto row1 = std::vector<TgBot::InlineKeyboardButton::Ptr>();
    auto row2 = std::vector<TgBot::InlineKeyboardButton::Ptr>();

    auto btn1 = std::make_shared<TgBot::InlineKeyboardButton>();
    btn1->text = "📅 Эта неделя";
    btn1->callbackData = "hist_curr_week";

    auto btn2 = std::make_shared<TgBot::InlineKeyboardButton>();
    btn2->text = "⏮ Прошлая неделя";
    btn2->callbackData = "hist_prev_week";

    auto btn3 = std::make_shared<TgBot::InlineKeyboardButton>();
    btn3->text = "🗓 Этот месяц";
    btn3->callbackData = "hist_curr_month";

    row1.push_back(btn1);
    row1.push_back(btn2);
    row2.push_back(btn3);

    kbd->inlineKeyboard.push_back(row1);
    kbd->inlineKeyboard.push_back(row2);
    return kbd;
}

void BotCore::weeklyReportScheduler() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));

        std::time_t now = std::time(nullptr);
        std::tm* tm_info = std::localtime(&now);

        if (tm_info->tm_wday == 1 && tm_info->tm_hour == 6 && tm_info->tm_min == 0) {
            std::time_t this_week_start = TimeUtils::getStartOfWeek(now);
            std::time_t prev_week_start = this_week_start - (7 * 24 * 3600);

            std::string week_id = std::to_string(prev_week_start);

            if (db.getSetting("last_sent_weekly_report") != week_id) {
                sendWeeklyReports(prev_week_start);
                db.setSetting("last_sent_weekly_report", week_id);
            }
        }
    }
}

void BotCore::sendWeeklyReports(int64_t prev_week_start) {
    fmt::print("⏰ Запуск автоматической рассылки недельных отчетов...\n");

    int64_t prev_week_end = prev_week_start + (7 * 24 * 3600) - 1;
    auto users = db.getAllUsers();

    for (const auto& user : users) {
        try {
            auto shifts = db.getShiftsInRange(user.id, prev_week_start, prev_week_end);
            auto report = TimeUtils::calculateReport(shifts, user);

            std::string text_report = fmt::format(
                "⏰ <b>Еженедельный автоматический отчет</b>\n"
                "Период: прошлая неделя\n"
                "───────────────────\n"
                "👤 Сотрудник: <b>{}</b>\n"
                "📅 Отработано смен: <b>{}</b>\n"
                "⏱ Всего часов: <b>{:.2f} ч.</b>\n\n"
                "💳 <b>Заработок:</b>\n"
                "• Оклад ({} руб/ч): {:.2f} руб.\n",
                user.name, report.shift_count, report.total_hours, user.base_rate, report.base_earnings
            );

            if (user.has_bonus) {
                text_report += fmt::format("• Надбавка (25 руб/ч): {:.2f} руб.\n", report.bonus_earnings);
            }

            if (report.premium > 0) {
                text_report += fmt::format("• 🎁 Премия (ПТ+СБ отработаны): <b>{:.2f} руб.</b>\n", report.premium);
            } else if (report.shift_count >= 5) {
                text_report += "• 🎁 Премия: <i>нет (не отработаны ПТ или СБ)</i>\n";
            }

            text_report += fmt::format("───────────────────\n💵 <b>ИТОГО ЗАРАБОТАНО: {:.2f} руб.</b>", report.grand_total);

            bot.getApi().sendMessage(user.id, text_report, nullptr, nullptr, nullptr, "HTML");
            fmt::print("✅ Отчет успешно отправлен пользователю {} ({})\n", user.name, user.id);

        } catch (const std::exception& e) {
            fmt::print(stderr, "❌ Не удалось отправить отчет пользователю {} (ID: {}): {}\n", user.name, user.id, e.what());
        }
    }
}

void BotCore::run() {
    fmt::print("Имя бота: {}\n", bot.getApi().getMe()->firstName);

    std::thread(&BotCore::weeklyReportScheduler, this).detach();
    fmt::print("⏰ Планировщик отчетов запущен в фоновом потоке.\n");

    fmt::print("Бот запущен и ждет сообщений...\n");

    auto allowedUpdates = std::make_shared<std::vector<std::string>>();
    allowedUpdates->push_back("message");
    allowedUpdates->push_back("callback_query");

    TgBot::TgLongPoll longPoll(bot, 100, 20, allowedUpdates);
    while (true) {
        try { longPoll.start(); }
        catch (TgBot::TgException& e) { fmt::print(stderr, "Ошибка сети: {}\n", e.what()); }
    }
}