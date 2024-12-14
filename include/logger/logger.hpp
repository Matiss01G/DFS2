#ifndef DFS_LOGGER_HPP
#define DFS_LOGGER_HPP

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>

namespace dfs::crypto {

class Logger {
public:
    static void init(const std::string& log_file = "dfs_crypto.log") {
        namespace logging = boost::log;
        namespace keywords = boost::log::keywords;
        namespace expr = boost::log::expressions;

        // Add common attributes
        logging::add_common_attributes();

        // Setup file sink
        logging::add_file_log(
            keywords::file_name = log_file,
            keywords::auto_flush = true,
            keywords::format = (
                expr::stream
                    << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S")
                    << "] [" << logging::trivial::severity
                    << "] " << expr::smessage
            )
        );

        // Set the minimum severity level
        logging::core::get()->set_filter(
            logging::trivial::severity >= logging::trivial::trace
        );
    }

    static boost::log::sources::severity_logger<boost::log::trivial::severity_level>& get_logger() {
        static boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger;
        return logger;
    }
};

} // namespace dfs::crypto

// Convenience macros for logging
#define DFS_LOG_TRACE BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::trace)
#define DFS_LOG_DEBUG BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::debug)
#define DFS_LOG_INFO BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::info)
#define DFS_LOG_WARN BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::warning)
#define DFS_LOG_ERROR BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::error)
#define DFS_LOG_FATAL BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::fatal)

#endif // DFS_LOGGER_HPP