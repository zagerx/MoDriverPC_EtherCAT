#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

namespace mo_ecat {

enum class LogLevel {
    Debug = 0,
    Info,
    Warn,
    Error,
    Fatal,
};

// 简单日志器：控制台 + 可选文件，线程安全。
class Logger {
public:
    static Logger &GetInstance();

    void SetConsoleLevel(LogLevel level);
    void SetFileLevel(LogLevel level);

    // 设置日志文件路径，空字符串表示不输出到文件。
    void SetLogFile(const std::string &path);

    void Log(LogLevel level, const char *file, int line, const std::string &message);

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    static const char *LevelToString(LogLevel level);
    static std::string GetTimestamp();

    std::mutex mutex_;
    std::ofstream file_stream_;
    LogLevel console_level_ = LogLevel::Info;
    LogLevel file_level_ = LogLevel::Debug;
};

// 流式日志辅助类，在析构时把缓冲内容一次性写入 Logger。
class LogStream {
public:
    LogStream(LogLevel level, const char *file, int line);
    ~LogStream();

    template <typename T>
    LogStream &operator<<(const T &value) {
        buffer_ << value;
        return *this;
    }

private:
    LogLevel level_;
    const char *file_;
    int line_;
    std::ostringstream buffer_;
};

// 用于编译时裁剪日志的 no-op 流。
class NullLogStream {
public:
    template <typename T>
    NullLogStream &operator<<(const T &) {
        return *this;
    }
};

} // namespace mo_ecat

// 编译时日志级别：0=Debug, 1=Info, 2=Warn, 3=Error, 4=Fatal, 5=无日志。
// 可通过 CMake 的 target_compile_definitions 覆盖，例如 -DMO_ECAT_LOG_LEVEL=2。
#ifndef MO_ECAT_LOG_LEVEL
#define MO_ECAT_LOG_LEVEL 1
#endif

#if MO_ECAT_LOG_LEVEL <= 0
#define LOG_DEBUG ::mo_ecat::LogStream(::mo_ecat::LogLevel::Debug, __FILE__, __LINE__)
#else
#define LOG_DEBUG ::mo_ecat::NullLogStream()
#endif

#if MO_ECAT_LOG_LEVEL <= 1
#define LOG_INFO ::mo_ecat::LogStream(::mo_ecat::LogLevel::Info, __FILE__, __LINE__)
#else
#define LOG_INFO ::mo_ecat::NullLogStream()
#endif

#if MO_ECAT_LOG_LEVEL <= 2
#define LOG_WARN ::mo_ecat::LogStream(::mo_ecat::LogLevel::Warn, __FILE__, __LINE__)
#else
#define LOG_WARN ::mo_ecat::NullLogStream()
#endif

#if MO_ECAT_LOG_LEVEL <= 3
#define LOG_ERROR ::mo_ecat::LogStream(::mo_ecat::LogLevel::Error, __FILE__, __LINE__)
#else
#define LOG_ERROR ::mo_ecat::NullLogStream()
#endif

#if MO_ECAT_LOG_LEVEL <= 4
#define LOG_FATAL ::mo_ecat::LogStream(::mo_ecat::LogLevel::Fatal, __FILE__, __LINE__)
#else
#define LOG_FATAL ::mo_ecat::NullLogStream()
#endif
