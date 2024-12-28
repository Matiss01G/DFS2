#ifndef DFS_TEST_UTILS_HPP
#define DFS_TEST_UTILS_HPP

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

// Set logging severity level
inline void init_logging() {
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= boost::log::trivial::error // Only show errors
    );
}

#endif // DFS_TEST_UTILS_HPP
