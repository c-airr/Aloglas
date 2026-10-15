#include "app/log.h"

#include <chrono>
#include <ctime>

namespace aloglas {

void Log::add(LogLevel level, std::string text) {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    localtime_s(&local, &now);  // the thread-safe variant of localtime on Windows
    char time[16];
    std::strftime(time, sizeof time, "%H:%M:%S", &local);
    add_at(time, level, std::move(text));
}

void Log::add_at(std::string time, LogLevel level, std::string text) {
    lines_.push_back({std::move(time), level, std::move(text)});
    if (lines_.size() > kMaxLines) lines_.pop_front();
    ++total_;
    if (on_change) on_change();
}

void Log::clear() {
    lines_.clear();
    if (on_change) on_change();
}

}  // namespace aloglas
