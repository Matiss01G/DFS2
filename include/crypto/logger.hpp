#ifndef DFS_CRYPTO_LOGGER_HPP
#define DFS_CRYPTO_LOGGER_HPP

#include <boost/log/trivial.hpp>
#include <string>

namespace dfs::crypto::logging {

/**
 * Initialize the logging system
 * @param node_addr The address identifier for this node
 */
void init_logging(const std::string& node_addr);

/**
 * Set the minimum severity level for logging
 */
void set_log_level(boost::log::trivial::severity_level level);

/**
 * Enable or disable logging globally
 */
void enable_logging();
void disable_logging();

} // namespace dfs::crypto::logging

// Convenience macros for logging
#define LOG_TRACE BOOST_LOG_TRIVIAL(trace)
#define LOG_DEBUG BOOST_LOG_TRIVIAL(debug)
#define LOG_INFO BOOST_LOG_TRIVIAL(info)
#define LOG_WARN BOOST_LOG_TRIVIAL(warning)
#define LOG_ERROR BOOST_LOG_TRIVIAL(error)
#define LOG_FATAL BOOST_LOG_TRIVIAL(fatal)

#endif // DFS_CRYPTO_LOGGER_HPP
