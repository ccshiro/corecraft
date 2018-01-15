#include "logging.h"
#include <cstdarg>
#include <cstdio>

std::mutex _printf_mutex; // Used for I/O
std::mutex _root_mutex;   // Used when creating new loggers

Logging logging;

Logger::Logger(std::string name, Logger* parent)
  : name_(name), parent_(parent), level_(LogLevel::parent)
{
    // children_.set_empty_key("");
}

Logger::~Logger()
{
    for (auto& c : children_)
        delete c.second;
}

void Logger::debug_valist(const char* fmtstr, va_list list)
{
    if (get_level() > LogLevel::debug)
        return;

    std::string str = "DEBUG:" + name_ + ":" + fmtstr + "\n";
    {
        std::lock_guard<std::mutex> guard(_printf_mutex);
        vprintf(str.c_str(), list);
    }
}

void Logger::info_valist(const char* fmtstr, va_list list)
{
    if (get_level() > LogLevel::info)
        return;

    std::string str = "INFO:" + name_ + ":" + fmtstr + "\n";

    {
        std::lock_guard<std::mutex> guard(_printf_mutex);
        vprintf(str.c_str(), list);
    }
}

void Logger::warning_valist(const char* fmtstr, va_list list)
{
    if (get_level() > LogLevel::warning)
        return;

    std::string str = "WARNING:" + name_ + ":" + fmtstr + "\n";

    {
        std::lock_guard<std::mutex> guard(_printf_mutex);
        vprintf(str.c_str(), list);
    }
}

void Logger::error_valist(const char* fmtstr, va_list list)
{
    if (get_level() > LogLevel::error)
        return;

    std::string str = "ERROR:" + name_ + ":" + fmtstr + "\n";

    {
        std::lock_guard<std::mutex> guard(_printf_mutex);
        vprintf(str.c_str(), list);
    }
}

void Logger::critical_valist(const char* fmtstr, va_list list)
{
    if (get_level() > LogLevel::critical)
        return;

    std::string str = "CRITICAL:" + name_ + ":" + fmtstr + "\n";

    {
        std::lock_guard<std::mutex> guard(_printf_mutex);
        vprintf(str.c_str(), list);
    }
}

void Logger::debug(const char* fmtstr, ...)
{
    va_list list;
    va_start(list, fmtstr);
    debug_valist(fmtstr, list);
    va_end(list);
}

void Logger::info(const char* fmtstr, ...)
{
    va_list list;
    va_start(list, fmtstr);
    info_valist(fmtstr, list);
    va_end(list);
}

void Logger::warning(const char* fmtstr, ...)
{
    va_list list;
    va_start(list, fmtstr);
    warning_valist(fmtstr, list);
    va_end(list);
}

void Logger::error(const char* fmtstr, ...)
{
    va_list list;
    va_start(list, fmtstr);
    error_valist(fmtstr, list);
    va_end(list);
}

void Logger::critical(const char* fmtstr, ...)
{
    va_list list;
    va_start(list, fmtstr);
    critical_valist(fmtstr, list);
    va_end(list);
}

void Logger::set_level(LogLevel level)
{
    std::lock_guard<std::mutex> guard(level_mutex_);
    level_ = level;
}

LogLevel Logger::get_level()
{
    std::lock_guard<std::mutex> guard(level_mutex_);
    if (level_ == LogLevel::parent)
        return parent_ ? parent_->get_level() : LogLevel::ignored;
    return level_;
}

Logger& Logger::_get_child(std::string identifier, std::string fullname)
{
    if (identifier.empty())
        throw std::length_error("Identifier cannot be empty");

    auto itr = children_.find(identifier);
    if (itr == children_.end())
    {
        children_[identifier] = new Logger(fullname, this);
        return *children_[identifier];
    }
    return *itr->second;
}

Logger& RootLogger::get_logger(const std::string& identifier)
{
    std::lock_guard<std::mutex> guard(_root_mutex);

    Logger* logger = this;

    if (identifier == "root")
        return *logger;

    std::size_t begin = 0, end = identifier.size();

    for (auto end = identifier.find("."); end != std::string::npos;
         end = identifier.find(".", end + 1))
    {
        logger = &logger->_get_child(
            identifier.substr(begin, end - begin), identifier.substr(0, end));
        begin = end + 1;
    }

    logger =
        &logger->_get_child(identifier.substr(begin, end - begin), identifier);

    return *logger;
}

RootLogger& Logging::get_logger()
{
    static RootLogger logger;
    return logger;
}

Logger& Logging::get_logger(const std::string& identifier)
{
    return get_logger().get_logger(identifier);
}

void Logging::debug(const char* fmtstr, ...)
{
    va_list ap;
    va_start(ap, fmtstr);
    get_logger().debug_valist(fmtstr, ap);
    va_end(ap);
}

void Logging::info(const char* fmtstr, ...)
{
    va_list ap;
    va_start(ap, fmtstr);
    get_logger().info_valist(fmtstr, ap);
    va_end(ap);
}

void Logging::warning(const char* fmtstr, ...)
{
    va_list ap;
    va_start(ap, fmtstr);
    get_logger().warning_valist(fmtstr, ap);
    va_end(ap);
}

void Logging::error(const char* fmtstr, ...)
{
    va_list ap;
    va_start(ap, fmtstr);
    get_logger().error_valist(fmtstr, ap);
    va_end(ap);
}

void Logging::critical(const char* fmtstr, ...)
{
    va_list ap;
    va_start(ap, fmtstr);
    get_logger().critical_valist(fmtstr, ap);
    va_end(ap);
}
