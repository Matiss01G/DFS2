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

namespace dfs::crypto {

// Define the global logger
BOOST_LOG_GLOBAL_LOGGER_INIT(global_logger, logger_type) {
    logger_type logger;
    
    // Add attributes
    logger.add_attribute("ThreadID", boost::log::attributes::current_thread_id());
    logger.add_attribute("Scope", boost::log::attributes::named_scope());
    
    return logger;
}

void init_logging(const std::string& log_file) {
    // Create a text file sink
    typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> file_sink;
    boost::shared_ptr<file_sink> sink = boost::make_shared<file_sink>(
        boost::log::keywords::file_name = log_file,
        boost::log::keywords::rotation_size = 10 * 1024 * 1024,  // 10 MB
        boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
        boost::log::keywords::format = (
            boost::log::expressions::stream
                << "[" << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                << "] [" << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID")
                << "] [" << boost::log::expressions::attr<severity_level>("Severity")
                << "] " << boost::log::expressions::message
        )
    );

    // Set up the core
    boost::log::core::get()->add_sink(sink);
    boost::log::add_common_attributes();
    
    // Enable automatic scope tracking
    boost::log::core::get()->add_global_attribute(
        "Scope", boost::log::attributes::named_scope()
    );
}

} // namespace dfs::crypto
