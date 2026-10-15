// Messages for the LOG panel (audio opened/lost, samples, saving, ...).
// UI thread only.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>

namespace aloglas {

enum class LogLevel { Info, Warning, Error };

struct LogLine {
    std::string time;  // "hh:mm:ss", local time
    LogLevel level = LogLevel::Info;
    std::string text;
};

class Log {
public:
    static constexpr std::size_t kMaxLines = 500;

    void add(LogLevel level, std::string text);
    void add_at(std::string time, LogLevel level, std::string text);
    void clear();

    const std::deque<LogLine>& lines() const { return lines_; }
    // Every line ever added (also the dropped and cleared ones).
    uint64_t total_added() const { return total_; }

    std::function<void()> on_change;

private:
    std::deque<LogLine> lines_;
    uint64_t total_ = 0;
};

}  // namespace aloglas
