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
#include <boost/log/attributes/named_scope.hpp>

namespace dfs::crypto {

// Define severity levels
enum class severity_level {
    trace,
    debug,
    info,
    warning,
    error,
    fatal
};

class Logger {
public:
    static void init(const std::string& log_file = "dfs_crypto.log") {
        namespace logging = boost::log;
        namespace keywords = boost::log::keywords;
        namespace expr = boost::log::expressions;

        // Add common attributes
        logging::add_common_attributes();
        logging::core::get()->add_global_attribute(
            "Scope", logging::attributes::named_scope());

        // Setup file sink
        logging::add_file_log(
            keywords::file_name = log_file,
            keywords::format = (
                expr::stream
                    << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                    << " [" << expr::attr<logging::trivial::severity_level>("Severity") << "]"
                    << " [Thread " << expr::attr<logging::attributes::current_thread_id::value_type>("ThreadID") << "]"
                    << " [" << expr::attr<std::string>("Process") << "]"
                    << " " << expr::smessage
            ),
            keywords::rotation_size = 10 * 1024 * 1024,  // 10 MB
            keywords::auto_flush = true
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

// Convenience macros for logging
#define LOG_TRACE BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::trace)
#define LOG_DEBUG BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::debug)
#define LOG_INFO BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::info)
#define LOG_WARN BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::warning)
#define LOG_ERROR BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::error)
#define LOG_FATAL BOOST_LOG_SEV(dfs::crypto::Logger::get_logger(), boost::log::trivial::fatal)

} // namespace dfs::crypto

#endif // DFS_LOGGER_HPP
