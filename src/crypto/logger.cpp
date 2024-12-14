 #pragma once
/**
 * @file logging.hpp
 * @brief Centralized logging configuration for the distributed file system
 *
 * Provides thread-safe initialization and configuration of the logging system.
 * Ensures logging is initialized only once and provides consistent formatting
 * across all components. Uses Boost.Log for the underlying implementation.
 */
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <mutex>
#include <string>

namespace dfs {
namespace logging {

// Static variables for initialization control
static std::mutex init_mutex;
static bool logging_initialized = false;

inline void init_logging(const std::string &node_addr) {
    std::lock_guard<std::mutex> lock(init_mutex);

    if (!logging_initialized) {
        // Add common attributes like TimeStamp, LineID, ProcessID
        boost::log::add_common_attributes();

        // Setup console output
        auto console = boost::log::add_console_log();
        console->set_formatter(
            boost::log::expressions::stream
                << "[" << node_addr << "] "
                << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
                    "TimeStamp", "%Y-%m-%d %H:%M:%S")
                << " [" << boost::log::trivial::severity << "] "
                << boost::log::expressions::smessage
        );

        // Setup file output with same format
        auto file = boost::log::add_file_log(
            boost::log::keywords::file_name = "logs/dfs_%N.log",
            boost::log::keywords::rotation_size = 10 * 1024 * 1024, // 10MB
            boost::log::keywords::auto_flush = true
        );
        file->set_formatter(
            boost::log::expressions::stream
                << "[" << node_addr << "] "
                << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
                    "TimeStamp", "%Y-%m-%d %H:%M:%S")
                << " [" << boost::log::trivial::severity << "] "
                << boost::log::expressions::smessage
        );

        logging_initialized = true;
    }
}

inline void set_log_level(boost::log::trivial::severity_level level) {
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= level
    );
}

/**
 * @brief Enable or disable logging globally
 */
inline void enable_logging() {
    boost::log::core::get()->set_logging_enabled(true);
}

inline void disable_logging() {
    boost::log::core::get()->set_logging_enabled(false);
}

} // namespace logging
} // namespace dfs

// Convenience macros for logging
#define LOG_TRACE BOOST_LOG_TRIVIAL(trace)
#define LOG_DEBUG BOOST_LOG_TRIVIAL(debug)
#define LOG_INFO BOOST_LOG_TRIVIAL(info)
#define LOG_WARN BOOST_LOG_TRIVIAL(warning)
#define LOG_ERROR BOOST_LOG_TRIVIAL(error)
#define LOG_FATAL BOOST_LOG_TRIVIAL(fatal)