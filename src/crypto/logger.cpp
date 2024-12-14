#include "crypto/logger.hpp"
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <mutex>
#include <string>

namespace dfs::crypto::logging {

static std::mutex init_mutex;
static bool logging_initialized = false;

void init_logging(const std::string& node_addr) {
    std::lock_guard<std::mutex> lock(init_mutex);

    if (!logging_initialized) {
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

        // Setup file output
        auto file = boost::log::add_file_log(
            boost::log::keywords::file_name = "logs/dfs_%N.log",
            boost::log::keywords::rotation_size = 10 * 1024 * 1024,
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

void set_log_level(boost::log::trivial::severity_level level) {
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= level
    );
}

void enable_logging() {
    boost::log::core::get()->set_logging_enabled(true);
}

void disable_logging() {
    boost::log::core::get()->set_logging_enabled(false);
}

} // namespace dfs::crypto::logging
