#ifndef DFS_TEST_UTILS_HPP
#define DFS_TEST_UTILS_HPP

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>

// Set logging severity level and configure logging
inline void init_logging() {
    // Remove any existing sinks to prevent duplicates
    boost::log::core::get()->remove_all_sinks();

    // Add console output with formatting
    boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");

    auto console_sink = boost::log::add_console_log(
        std::cout,
        boost::log::keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%",
        boost::log::keywords::auto_flush = true
    );

    // Set logging level to debug to see all messages
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= boost::log::trivial::debug
    );

    // Add commonly used attributes
    boost::log::add_common_attributes();
}

#endif // DFS_TEST_UTILS_HPP