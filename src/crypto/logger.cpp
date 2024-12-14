#include "crypto/logger.hpp"
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <mutex>
#include <filesystem>
#include <fstream>

namespace dfs::crypto::logging {

static std::mutex init_mutex;
static bool logging_initialized = false;

void init_logging(const std::string& node_addr) {
    std::lock_guard<std::mutex> lock(init_mutex);
    
    if (logging_initialized) {
        return;
    }

    try {
        // Create logs directory
        std::filesystem::create_directories("logs");

        // Add common attributes
        boost::log::add_common_attributes();

        // Set up console sink
        auto console_sink = boost::log::add_console_log(
            std::cout,
            boost::log::keywords::format = (
                boost::log::expressions::stream
                    << "[" << node_addr << "] "
                    << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
                        "TimeStamp", "%Y-%m-%d %H:%M:%S")
                    << " [" << boost::log::trivial::severity << "] "
                    << boost::log::expressions::smessage
            ),
            boost::log::keywords::auto_flush = true
        );

        // Set up file sink
        auto file_sink = boost::log::add_file_log(
            boost::log::keywords::file_name = "logs/dfs_%N.log",
            boost::log::keywords::rotation_size = 10 * 1024 * 1024,
            boost::log::keywords::open_mode = std::ios_base::app,
            boost::log::keywords::auto_flush = true,
            boost::log::keywords::format = (
                boost::log::expressions::stream
                    << "[" << node_addr << "] "
                    << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
                        "TimeStamp", "%Y-%m-%d %H:%M:%S")
                    << " [" << boost::log::trivial::severity << "] "
                    << boost::log::expressions::smessage
            )
        );

        // Set default severity level to trace
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::trace
        );

        logging_initialized = true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize logging: " << e.what() << std::endl;
        throw;
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