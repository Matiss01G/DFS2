#ifndef DFS_CRYPTO_LOGGER_HPP
#define DFS_CRYPTO_LOGGER_HPP

#include <boost/log/trivial.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

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

// Declare the logger type
using logger_type = boost::log::sources::severity_logger<severity_level>;

// Declare the global logger storage
BOOST_LOG_GLOBAL_LOGGER(global_logger, logger_type)

// Initialize logging system
void init_logging(const std::string& log_file = "crypto.log");

} // namespace dfs::crypto

// Convenience macros for logging
#define LOG_TRACE BOOST_LOG_SEV(dfs::crypto::global_logger::get(), dfs::crypto::severity_level::trace)
#define LOG_DEBUG BOOST_LOG_SEV(dfs::crypto::global_logger::get(), dfs::crypto::severity_level::debug)
#define LOG_INFO BOOST_LOG_SEV(dfs::crypto::global_logger::get(), dfs::crypto::severity_level::info)
#define LOG_WARN BOOST_LOG_SEV(dfs::crypto::global_logger::get(), dfs::crypto::severity_level::warning)
#define LOG_ERROR BOOST_LOG_SEV(dfs::crypto::global_logger::get(), dfs::crypto::severity_level::error)
#define LOG_FATAL BOOST_LOG_SEV(dfs::crypto::global_logger::get(), dfs::crypto::severity_level::fatal)

#endif // DFS_CRYPTO_LOGGER_HPP
