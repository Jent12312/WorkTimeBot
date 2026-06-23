#pragma once
#include <ctime>
#include <vector>
#include "Database.hpp"

namespace TimeUtils {

    // Структура для итогового расчета за период
    struct WorkReport {
        int shift_count = 0;
        double total_hours = 0.0;
        double base_earnings = 0.0;
        double bonus_earnings = 0.0; // 25 руб/час
        double premium = 0.0;        // Еженедельная премия (3000, 3500, 4000)
        double grand_total = 0.0;
        bool has_friday = false;
        bool has_saturday = false;
    };

    // Получить начало дня (00:00:00) для переданного timestamp
    inline std::time_t getStartOfDay(std::time_t t) {
        std::tm* tm_info = std::localtime(&t);
        tm_info->tm_hour = 0;
        tm_info->tm_min = 0;
        tm_info->tm_sec = 0;
        return std::mktime(tm_info);
    }

    // Получить начало текущей недели (Понедельник 00:00:00)
    inline std::time_t getStartOfWeek(std::time_t t) {
        std::tm* tm_info = std::localtime(&t);
        // tm_wday: 0 = воскресенье, 1 = понедельник ... 6 = суббота
        int days_since_monday = tm_info->tm_wday - 1;
        if (days_since_monday < 0) days_since_monday = 6; // Если воскресенье
        
        t -= days_since_monday * 24 * 3600;
        return getStartOfDay(t);
    }

    // Получить начало месяца (1 число 00:00:00)
    inline std::time_t getStartOfMonth(std::time_t t) {
        std::tm* tm_info = std::localtime(&t);
        tm_info->tm_mday = 1;
        tm_info->tm_hour = 0;
        tm_info->tm_min = 0;
        tm_info->tm_sec = 0;
        return std::mktime(tm_info);
    }

    // Расчет отчета и премий по списку смен
    inline WorkReport calculateReport(const std::vector<Shift>& shifts, const User& user) {
        WorkReport report;
        report.shift_count = static_cast<int>(shifts.size());

        for (const auto& shift : shifts) {
            double hours = (shift.end_time - shift.start_time) / 3600.0;
            report.total_hours += hours;
            report.base_earnings += hours * user.base_rate;
            if (user.has_bonus) {
                report.bonus_earnings += hours * 25.0;
            }

            // Проверяем день недели (для премии важны пятница и суббота)
            std::time_t start_t = static_cast<std::time_t>(shift.start_time);
            std::tm* tm_info = std::localtime(&start_t);
            if (tm_info->tm_wday == 5) report.has_friday = true;   // Пятница
            if (tm_info->tm_wday == 6) report.has_saturday = true; // Суббота
        }

        // Логика еженедельной премии: минимум 5 смен, обязательно ПТ и СБ
        if (report.shift_count >= 5 && report.has_friday && report.has_saturday) {
            if (report.shift_count == 5) report.premium = 3000.0;
            else if (report.shift_count == 6) report.premium = 3500.0;
            else if (report.shift_count >= 7) report.premium = 4000.0;
        }

        report.grand_total = report.base_earnings + report.bonus_earnings + report.premium;
        return report;
    }
}
