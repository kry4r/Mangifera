#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <memory>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace mango::core {

    enum class LogLevel {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    enum class ConsoleColor {
        RESET = 0,
        BLACK = 30,
        RED = 31,
        GREEN = 32,
        YELLOW = 33,
        BLUE = 34,
        MAGENTA = 35,
        CYAN = 36,
        WHITE = 37,
        BRIGHT_BLACK = 90,
        BRIGHT_RED = 91,
        BRIGHT_GREEN = 92,
        BRIGHT_YELLOW = 93,
        BRIGHT_BLUE = 94,
        BRIGHT_MAGENTA = 95,
        BRIGHT_CYAN = 96,
        BRIGHT_WHITE = 97
    };

    struct LogEntry {
        LogLevel level;
        std::string message;
        std::string timestamp;
        std::string file;
        int line;
        std::string function;
    };

    class UkaLogger {
    public:
        static UkaLogger& instance();

        UkaLogger(const UkaLogger&) = delete;
        UkaLogger& operator=(const UkaLogger&) = delete;

        ~UkaLogger();

        void set_level(LogLevel level);
        void set_console_output(bool enabled);
        void set_file_output(bool enabled);
        void set_async_mode(bool enabled);
        void set_log_file_path(const std::string& path);
        void set_color_output(bool enabled);

        void log(LogLevel level, const std::string& message,
                const std::string& file = "", int line = 0, const std::string& function = "");

        template<typename... Args>
        void log_formatted(LogLevel level, const std::string& format,
                          const std::string& file, int line, const std::string& function, Args&&... args);

        void trace(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        void debug(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        void info(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        void warn(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        void error(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        void fatal(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");

        void flush();

    private:
        UkaLogger();
        void init_log_file();
        void worker_thread();
        void write_to_console(const LogEntry& entry);
        void write_to_file(const LogEntry& entry);
        std::string format_timestamp();
        std::string get_level_string(LogLevel level);
        std::string get_level_emoji(LogLevel level);
        ConsoleColor get_level_color(LogLevel level);
        std::string colorize(const std::string& text, ConsoleColor color);
        std::string extract_filename(const std::string& path);

        template<typename T>
        std::string to_string_helper(T&& val);

        template<typename... Args>
        std::string format_string(const std::string& format, Args&&... args);

        LogLevel current_level_;
        bool console_output_;
        bool file_output_;
        bool async_mode_;
        bool color_output_;
        std::string log_file_path_;

        std::mutex queue_mutex_;
        std::condition_variable queue_condition_;
        std::queue<LogEntry> log_queue_;

        std::atomic<bool> should_stop_;
        std::unique_ptr<std::thread> worker_thread_;

        std::ofstream log_file_;
        std::mutex file_mutex_;
        std::mutex console_mutex_;
    };

    template<typename... Args>
    void UkaLogger::log_formatted(LogLevel level, const std::string& format,
                                  const std::string& file, int line, const std::string& function, Args&&... args) {
        if (level < current_level_) return;

        std::string formatted_message = format_string(format, std::forward<Args>(args)...);
        log(level, formatted_message, file, line, function);
    }

    template<typename T>
    std::string UkaLogger::to_string_helper(T&& val) {
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            return val;
        } else if constexpr (std::is_same_v<std::decay_t<T>, const char*>) {
            return std::string(val);
        } else {
            std::ostringstream oss;
            oss << val;
            return oss.str();
        }
    }

    template<typename... Args>
    std::string UkaLogger::format_string(const std::string& format, Args&&... args) {
        if constexpr (sizeof...(args) == 0) {
            return format;
        } else {
            std::string result = format;
            size_t pos = 0;
            ((pos = result.find("{}", pos),
              pos != std::string::npos ?
              (result.replace(pos, 2, to_string_helper(std::forward<Args>(args))), pos += to_string_helper(std::forward<Args>(args)).length()) :
              pos), ...);
            return result;
        }
    }

}

#define UKA_LOG_TRACE(msg) mango::core::UkaLogger::instance().trace(msg, __FILE__, __LINE__, __FUNCTION__)
#define UKA_LOG_DEBUG(msg) mango::core::UkaLogger::instance().debug(msg, __FILE__, __LINE__, __FUNCTION__)
#define UKA_LOG_INFO(msg) mango::core::UkaLogger::instance().info(msg, __FILE__, __LINE__, __FUNCTION__)
#define UKA_LOG_WARN(msg) mango::core::UkaLogger::instance().warn(msg, __FILE__, __LINE__, __FUNCTION__)
#define UKA_LOG_ERROR(msg) mango::core::UkaLogger::instance().error(msg, __FILE__, __LINE__, __FUNCTION__)
#define UKA_LOG_FATAL(msg) mango::core::UkaLogger::instance().fatal(msg, __FILE__, __LINE__, __FUNCTION__)

#define UKA_LOG_TRACE_FMT(format, ...) mango::core::UkaLogger::instance().log_formatted(mango::core::LogLevel::TRACE, format, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define UKA_LOG_DEBUG_FMT(format, ...) mango::core::UkaLogger::instance().log_formatted(mango::core::LogLevel::DEBUG, format, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define UKA_LOG_INFO_FMT(format, ...) mango::core::UkaLogger::instance().log_formatted(mango::core::LogLevel::INFO, format, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define UKA_LOG_WARN_FMT(format, ...) mango::core::UkaLogger::instance().log_formatted(mango::core::LogLevel::WARN, format, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define UKA_LOG_ERROR_FMT(format, ...) mango::core::UkaLogger::instance().log_formatted(mango::core::LogLevel::ERROR, format, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define UKA_LOG_FATAL_FMT(format, ...) mango::core::UkaLogger::instance().log_formatted(mango::core::LogLevel::FATAL, format, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define UH_TRACE(msg) UKA_LOG_TRACE(msg)
#define UH_DEBUG(msg) UKA_LOG_DEBUG(msg)
#define UH_INFO(msg) UKA_LOG_INFO(msg)
#define UH_WARN(msg) UKA_LOG_WARN(msg)
#define UH_ERROR(msg) UKA_LOG_ERROR(msg)
#define UH_FATAL(msg) UKA_LOG_FATAL(msg)

#define UH_TRACE_FMT(format, ...) UKA_LOG_TRACE_FMT(format, __VA_ARGS__)
#define UH_DEBUG_FMT(format, ...) UKA_LOG_DEBUG_FMT(format, __VA_ARGS__)
#define UH_INFO_FMT(format, ...) UKA_LOG_INFO_FMT(format, __VA_ARGS__)
#define UH_WARN_FMT(format, ...) UKA_LOG_WARN_FMT(format, __VA_ARGS__)
#define UH_ERROR_FMT(format, ...) UKA_LOG_ERROR_FMT(format, __VA_ARGS__)
#define UH_FATAL_FMT(format, ...) UKA_LOG_FATAL_FMT(format, __VA_ARGS__)

#define ULOG_I(msg) UH_INFO(msg)
#define ULOG_W(msg) UH_WARN(msg)
#define ULOG_E(msg) UH_ERROR(msg)
#define ULOG_D(msg) UH_DEBUG(msg)
#define ULOG_T(msg) UH_TRACE(msg)
#define ULOG_F(msg) UH_FATAL(msg)
