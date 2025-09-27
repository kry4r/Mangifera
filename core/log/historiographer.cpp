#include "historiographer.hpp"
#include <iostream>
#include <ctime>

namespace mango::core {

UkaLogger& UkaLogger::instance() {
    static UkaLogger instance;
    return instance;
}

UkaLogger::UkaLogger()
    : current_level_(LogLevel::INFO)
    , console_output_(true)
    , file_output_(true)
    , async_mode_(true)
    , color_output_(true)
    , log_file_path_("logs/uka-historiographer.log")
    , should_stop_(false) {

    init_log_file();

    if (async_mode_) {
        worker_thread_ = std::make_unique<std::thread>(&UkaLogger::worker_thread, this);
    }
}

UkaLogger::~UkaLogger() {
    if (async_mode_ && worker_thread_) {
        should_stop_ = true;
        queue_condition_.notify_all();
        if (worker_thread_->joinable()) {
            worker_thread_->join();
        }
    }

    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void UkaLogger::set_level(LogLevel level) {
    current_level_ = level;
}

void UkaLogger::set_console_output(bool enabled) {
    console_output_ = enabled;
}

void UkaLogger::set_file_output(bool enabled) {
    file_output_ = enabled;
    if (enabled && !log_file_.is_open()) {
        init_log_file();
    }
}

void UkaLogger::set_color_output(bool enabled) {
    color_output_ = enabled;
}

void UkaLogger::set_async_mode(bool enabled) {
    if (async_mode_ != enabled) {
        if (async_mode_ && worker_thread_) {
            should_stop_ = true;
            queue_condition_.notify_all();
            if (worker_thread_->joinable()) {
                worker_thread_->join();
            }
            worker_thread_.reset();
            should_stop_ = false;
        }

        async_mode_ = enabled;

        if (async_mode_) {
            worker_thread_ = std::make_unique<std::thread>(&UkaLogger::worker_thread, this);
        }
    }
}

void UkaLogger::set_log_file_path(const std::string& path) {
    log_file_path_ = path;
    if (file_output_) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
        init_log_file();
    }
}

void UkaLogger::init_log_file() {
    try {
        std::filesystem::path log_path(log_file_path_);
        std::filesystem::path log_dir = log_path.parent_path();

        if (!log_dir.empty() && !std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }

        log_file_.open(log_file_path_, std::ios::app);
        if (!log_file_.is_open()) {
            std::cerr << "UkaLogger: Failed to open log file: " << log_file_path_ << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "UkaLogger: Error initializing log file: " << e.what() << std::endl;
    }
}

void UkaLogger::log(LogLevel level, const std::string& message,
                   const std::string& file, int line, const std::string& function) {
    if (level < current_level_) {
        return;
    }

    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.timestamp = format_timestamp();
    entry.file = extract_filename(file);
    entry.line = line;
    entry.function = function;

    if (async_mode_) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        log_queue_.push(entry);
        queue_condition_.notify_one();
    } else {
        if (console_output_) {
            write_to_console(entry);
        }
        if (file_output_) {
            write_to_file(entry);
        }
    }
}

void UkaLogger::worker_thread() {
    while (!should_stop_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_condition_.wait(lock, [this] { return !log_queue_.empty() || should_stop_; });

        while (!log_queue_.empty()) {
            LogEntry entry = log_queue_.front();
            log_queue_.pop();
            lock.unlock();

            if (console_output_) {
                write_to_console(entry);
            }
            if (file_output_) {
                write_to_file(entry);
            }

            lock.lock();
        }
    }
}

void UkaLogger::write_to_console(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(console_mutex_);

    std::string level_str = get_level_string(entry.level);
    ConsoleColor color = get_level_color(entry.level);

    std::ostringstream oss;
    oss << "[" << entry.timestamp << "] ";

    if (color_output_) {
        oss << colorize("[" + level_str + "]", color);
    } else {
        oss << "[" + level_str + "]";
    }

    oss << " " << entry.message;

    if (!entry.file.empty() && entry.line > 0) {
        if (color_output_) {
            oss << " " << colorize("(" + entry.file + ":" + std::to_string(entry.line) + ")", ConsoleColor::BRIGHT_BLACK);
        } else {
            oss << " (" + entry.file + ":" + std::to_string(entry.line) + ")";
        }
    }

    std::cout << oss.str() << std::endl;
}

void UkaLogger::write_to_file(const LogEntry& entry) {
    if (!log_file_.is_open()) return;

    std::lock_guard<std::mutex> lock(file_mutex_);

    log_file_ << "[" << entry.timestamp << "] "
              << "[" << get_level_string(entry.level) << "] "
              << entry.message;

    if (!entry.file.empty() && entry.line > 0) {
        log_file_ << " (" << entry.file << ":" << entry.line;
        if (!entry.function.empty()) {
            log_file_ << " in " << entry.function << "()";
        }
        log_file_ << ")";
    }

    log_file_ << std::endl;
    log_file_.flush();
}

std::string UkaLogger::format_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string UkaLogger::get_level_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

ConsoleColor UkaLogger::get_level_color(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return ConsoleColor::BRIGHT_BLACK;
        case LogLevel::DEBUG: return ConsoleColor::CYAN;
        case LogLevel::INFO:  return ConsoleColor::GREEN;
        case LogLevel::WARN:  return ConsoleColor::YELLOW;
        case LogLevel::ERROR: return ConsoleColor::RED;
        case LogLevel::FATAL: return ConsoleColor::BRIGHT_RED;
        default: return ConsoleColor::WHITE;
    }
}

std::string UkaLogger::colorize(const std::string& text, ConsoleColor color) {
    return "\033[" + std::to_string(static_cast<int>(color)) + "m" + text + "\033[0m";
}

std::string UkaLogger::extract_filename(const std::string& path) {
    if (path.empty()) return "";

    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

void UkaLogger::flush() {
    if (async_mode_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        while (!log_queue_.empty()) {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            lock.lock();
        }
    }

    if (log_file_.is_open()) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        log_file_.flush();
    }

    std::cout.flush();
}

void UkaLogger::trace(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::TRACE, message, file, line, function);
}

void UkaLogger::debug(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::DEBUG, message, file, line, function);
}

void UkaLogger::info(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::INFO, message, file, line, function);
}

void UkaLogger::warn(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::WARN, message, file, line, function);
}

void UkaLogger::error(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::ERROR, message, file, line, function);
}

void UkaLogger::fatal(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::FATAL, message, file, line, function);
}

} // namespace uka::historiographer
