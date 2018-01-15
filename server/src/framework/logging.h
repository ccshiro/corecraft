#ifndef FRAMEWORK__LOGGING_H
#define FRAMEWORK__LOGGING_H

#include <sparsehash/dense_hash_map>
#include <mutex>
#include <string>
#include <map>

/* Logging functionality.
 * This logger is similar to the basic usage of Python's logging library. Some
 * features are currently lackning, most notably filters. But given log levels
 * and the fact there's a log-level ignored I woner whether we need filters at
 * all.
 *
 * Usage example:
 *
 * // Root Logger
 * logging.info("yellow");    // prints: INFO:root:yellow
 *
 * // User-specified Logger
 * auto& log = logging.get_logger("component");
 * log.warning("watch out!"); // prints: WARNING:component:watch out!
 *
 * // Multi-hierarchical Logger
 * auto& parent = logging.get_loger("parent");
 * auto& child = logging.get_logger("parent.child");
 * parent.set_level(LogLevel::warning)
 * child.info("hi") // prints nothing, parent has higher min level
 * child.warning("ho"); // prints: WARNING:parent.child:ho
 */

#if defined(__GNUC__) || defined(__clang__)
#define FMT_STRING_CHECK __attribute__((format(printf, 2, 3)))
#else
#define FMT_STRING_CHECK
#endif

enum class LogLevel
{
    parent = 0, // Inherit parents level
    debug = 1,
    info = 2,
    warning = 3,
    error = 4,
    critical = 5,
    ignored = 6
};

class Logger
{
public:
    Logger(std::string name, Logger* parent);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void debug_valist(const char* fmstr, va_list list);
    void info_valist(const char* fmstr, va_list list);
    void warning_valist(const char* fmstr, va_list list);
    void error_valist(const char* fmstr, va_list list);
    void critical_valist(const char* fmstr, va_list list);

    void debug(const char* fmtstr, ...) FMT_STRING_CHECK;
    void info(const char* fmtstr, ...) FMT_STRING_CHECK;
    void warning(const char* fmtstr, ...) FMT_STRING_CHECK;
    void error(const char* fmtstr, ...) FMT_STRING_CHECK;
    void critical(const char* fmtstr, ...) FMT_STRING_CHECK;

    void set_level(LogLevel level);
    LogLevel get_level();

    // Must hold _root_mutex to call
    Logger& _get_child(std::string identifier, std::string fullname);

protected:
    std::string name_;
    Logger* parent_;
    std::map<std::string, Logger*> children_;

    std::mutex level_mutex_;
    LogLevel level_;
};

// This would work better with a namespace and functions instead of a global
// instance, really... But I want my dot operator, gosh darn it!
class RootLogger : public Logger
{
public:
    RootLogger() : Logger("root", nullptr) { level_ = LogLevel::info; }

    Logger& get_logger(const std::string& identifier);
};

// NOTE:
// This proxy class is needed so that we won't have circular static declarations
// in a way that *may* break. I.e., if  we had RootLogger declared extern
// (extern RootLogger logging;), and then someone used
// logging.get_logger("smth") we'd have a possibility that RootLogger logging
// has not yet been constructed, which means we'll end up doing dereferencing of
// the hash_map internals that are not yet allocated and set up. This proxy
// class solves that (as it has no internal data).
struct Logging
{
    RootLogger& get_logger();
    Logger& get_logger(const std::string& identifier);

    void debug(const char* fmtstr, ...) FMT_STRING_CHECK;
    void info(const char* fmtstr, ...) FMT_STRING_CHECK;
    void warning(const char* fmtstr, ...) FMT_STRING_CHECK;
    void error(const char* fmtstr, ...) FMT_STRING_CHECK;
    void critical(const char* fmtstr, ...) FMT_STRING_CHECK;
};

extern Logging logging;

// Debug logging not available in optimized build
#ifndef OPTIMIZED_BUILD
#define LOG_DEBUG(log, ...)     \
    do                          \
    {                           \
        log.debug(__VA_ARGS__); \
    } while (0)
#else
#define LOG_DEBUG(log, ...)
#endif

#endif
