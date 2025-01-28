#pragma once

#include <sstream>
#include <functional>
#include <memory>
#include <boost/log/trivial.hpp>

namespace dfs {
namespace network {

/**
 * A StreamPipeline class that provides lazy, on-demand data streaming between components.
 * It uses a producer function to generate data only when needed, avoiding full data buffering.
 */
class StreamPipeline : public std::stringstream {
public:
    using ProducerFunc = std::function<bool(std::stringstream&)>;
    
    /**
     * Constructor that takes a producer function.
     * The producer function should write data to the provided stringstream and return true on success.
     */
    explicit StreamPipeline(ProducerFunc producer)
        : producer_(std::move(producer))
        , produced_(false)
        , buffer_size_(0)
        , current_pos_(0) {
        BOOST_LOG_TRIVIAL(debug) << "Created StreamPipeline";
    }

    /**
     * Copy constructor deleted to prevent unintended copying of stream state
     */
    StreamPipeline(const StreamPipeline&) = delete;
    StreamPipeline& operator=(const StreamPipeline&) = delete;

    /**
     * Move constructor and assignment operator
     */
    StreamPipeline(StreamPipeline&&) = default;
    StreamPipeline& operator=(StreamPipeline&&) = default;

protected:
    /**
     * Override of std::stringstream's sync method to implement lazy data production.
     * Called when the stream needs more data.
     */
    virtual int sync() override {
        if (!produced_) {
            BOOST_LOG_TRIVIAL(debug) << "StreamPipeline: Producing data on demand";
            std::stringstream temp;
            if (!producer_(temp)) {
                BOOST_LOG_TRIVIAL(error) << "StreamPipeline: Failed to produce data";
                setstate(std::ios::failbit);
                return -1;
            }
            
            str(temp.str());
            buffer_size_ = temp.str().size();
            produced_ = true;
            BOOST_LOG_TRIVIAL(debug) << "StreamPipeline: Produced " << buffer_size_ << " bytes";
        }
        return 0;
    }

    /**
     * Override seekoff to properly handle seeking in the stream
     */
    virtual std::streampos seekoff(
        std::streamoff off,
        std::ios_base::seekdir way,
        std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override {
        // Ensure data is produced before seeking
        if (sync() == -1) {
            return -1;
        }

        // Calculate new position
        switch (way) {
            case std::ios_base::beg:
                current_pos_ = off;
                break;
            case std::ios_base::cur:
                current_pos_ += off;
                break;
            case std::ios_base::end:
                current_pos_ = buffer_size_ + off;
                break;
            default:
                return -1;
        }

        // Validate position
        if (current_pos_ < 0 || current_pos_ > buffer_size_) {
            setstate(std::ios::failbit);
            return -1;
        }

        // Perform the actual seek
        std::streampos result = std::stringstream::seekoff(off, way, which);
        if (result == -1) {
            setstate(std::ios::failbit);
        }
        return result;
    }

private:
    ProducerFunc producer_;
    bool produced_;
    std::streamsize buffer_size_;
    std::streamoff current_pos_;
};

} // namespace network
} // namespace dfs
