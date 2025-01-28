#pragma once

#include <sstream>
#include <functional>
#include <memory>

namespace dfs {
namespace network {

/**
 * @brief A stream pipeline that lazily produces data on demand
 * 
 * This class allows chaining of stream operations without buffering the entire
 * data in memory. Data is produced only when read from the stream.
 */
class StreamPipeline : public std::stringstream {
public:
    using ProducerFunc = std::function<bool(std::stringstream&)>;

    /**
     * @brief Construct a new Stream Pipeline with a producer function
     * 
     * @param producer Function that generates data when called
     */
    explicit StreamPipeline(ProducerFunc producer)
        : producer_(std::move(producer)), produced_(false) {}

    /**
     * @brief Chain another operation to the pipeline
     * 
     * @param transform Function to transform the data
     * @return std::shared_ptr<StreamPipeline> New pipeline stage
     */
    std::shared_ptr<StreamPipeline> then(ProducerFunc transform) {
        auto next_producer = [this, transform](std::stringstream& output) -> bool {
            // Create temporary buffer for intermediate result
            std::stringstream temp;

            // Ensure current stage has produced data
            if (!this->sync() || !producer_(temp)) {
                return false;
            }

            // Reset temp stream position
            temp.seekg(0);

            // Apply transformation
            return transform(output);
        };
        return std::make_shared<StreamPipeline>(next_producer);
    }

protected:
    /**
     * @brief Override sync to implement lazy evaluation
     * 
     * This method is called by the stream when data needs to be read.
     * It ensures data is produced only once and only when needed.
     * 
     * @return int 0 on success, -1 on failure
     */
    int sync() override {
        if (!produced_) {
            std::stringstream temp;
            if (!producer_(temp)) {
                setstate(std::ios::failbit);
                return -1;
            }
            str(temp.str());
            produced_ = true;
        }
        return 0;
    }

private:
    ProducerFunc producer_;
    bool produced_;
};

} // namespace network
} // namespace dfs