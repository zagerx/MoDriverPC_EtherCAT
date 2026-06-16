#include "utils/logger.h"

#include <ctime>
#include <iostream>

namespace mo_ecat {

Logger &Logger::GetInstance()
{
    static Logger instance;
    return instance;
}

Logger::~Logger()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void Logger::SetConsoleLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    console_level_ = level;
}

void Logger::SetFileLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    file_level_ = level;
}

void Logger::SetLogFile(const std::string &path)
{
    if (path.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    file_stream_.open(path, std::ios::out | std::ios::app);
    if (!file_stream_.is_open()) {
        std::cerr << "[Logger] Failed to open log file: " << path << "\n";
    }
}

const char *Logger::LevelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    }
    return "UNKNOWN";
}

std::string Logger::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm{};
    localtime_r(&time_t_now, &local_tm);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S") << "." << std::setw(3)
        << std::setfill('0') << ms.count();
    return oss.str();
}

void Logger::Log(LogLevel level, const char *file, int line, const std::string &message)
{
    const bool log_console = static_cast<int>(level) >= static_cast<int>(console_level_);
    const bool log_file = file_stream_.is_open() && static_cast<int>(level) >= static_cast<int>(file_level_);
    if (!log_console && !log_file) {
        return;
    }

    // 取源文件短名，避免完整路径过长。
    const char *basename = file;
    for (const char *p = file; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            basename = p + 1;
        }
    }

    std::ostringstream oss;
    oss << "[" << GetTimestamp() << "] [" << LevelToString(level) << "] [" << basename << ":"
        << line << "] " << message << "\n";
    const std::string formatted = oss.str();

    std::lock_guard<std::mutex> lock(mutex_);
    if (log_console) {
        if (level >= LogLevel::Warn) {
            std::cerr << formatted;
        } else {
            std::cout << formatted;
        }
    }
    if (log_file && file_stream_.is_open()) {
        file_stream_ << formatted;
        file_stream_.flush();
    }
}

LogStream::LogStream(LogLevel level, const char *file, int line)
    : level_(level), file_(file), line_(line)
{
}

LogStream::~LogStream()
{
    Logger::GetInstance().Log(level_, file_, line_, buffer_.str());
}

} // namespace mo_ecat
