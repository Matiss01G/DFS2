#include "crypto/logger.hpp"
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <filesystem>
#include <sstream>

namespace dfs::crypto {

// Severity level to string conversion
// Severity level to string conversion
const char* to_string(severity_level level) {
    switch (level) {
        case severity_level::trace:   return "TRACE";
        case severity_level::debug:   return "DEBUG";
        case severity_level::info:    return "INFO";
        case severity_level::warning: return "WARNING";
        case severity_level::error:   return "ERROR";
        case severity_level::fatal:   return "FATAL";
        default:                      return "UNKNOWN";
    }
}

// Custom formatting operator for severity_level
std::ostream& operator<<(std::ostream& strm, severity_level level) {
    strm << to_string(level);
    return strm;
}

// Define the global logger
BOOST_LOG_GLOBAL_LOGGER_INIT(global_logger, logger_type) {
    logger_type logger;
    
    // Add common attributes
    logger.add_attribute("TimeStamp", boost::log::attributes::local_clock());
    logger.add_attribute("ThreadID", boost::log::attributes::current_thread_id());
    logger.add_attribute("ProcessID", boost::log::attributes::current_process_id());
    logger.add_attribute("Scope", boost::log::attributes::named_scope());
    
    return logger;
}

void init_logging(const std::string& log_file, severity_level min_level) {
    try {
        // Clear any existing sinks
        boost::log::core::get()->remove_all_sinks();

        // Create and configure text file sink backend
        auto backend = boost::make_shared<boost::log::sinks::text_file_backend>();
        
        // Convert to absolute path
        std::filesystem::path log_path = std::filesystem::absolute(log_file);
        backend->set_file_name_pattern(log_path.string());
        backend->set_open_mode(std::ios::out | std::ios::trunc);  // Start with a fresh log
        backend->auto_flush(true);

        // Create the sink
        using text_sink = boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>;
        auto sink = boost::make_shared<text_sink>(backend);

        // Set up the formatter
        namespace expr = boost::log::expressions;
        sink->set_formatter(
            expr::stream
                << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                << " [" << expr::attr<severity_level>("Severity") << "] "
                << expr::smessage
        );

        // Add sink and attributes to the core
        boost::log::core::get()->add_sink(sink);
        boost::log::add_common_attributes();
        
        // Enable logging with no minimum severity filter for tests
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
        boost::log::core::get()->set_logging_enabled(true);

        // Log initialization success using direct backend write
        std::stringstream ss;
        ss << "Logging system initialized with file: " << log_path.string();
        backend->consume(boost::log::record_view(), ss.str());
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize logging: " << e.what() << std::endl;
        throw;
    }
}

} // namespace dfs::crypto
