#include "utils/pipeliner.hpp"

namespace dfs {
namespace utils {

PipelinerPtr Pipeliner::create(ProducerFn producer) {
    return std::make_shared<Pipeliner>(producer);
}

Pipeliner::Pipeliner(ProducerFn producer)
    : producer_(producer)
    , buffer_size_(8192)  // Default 8KB buffer
    , produced_(false) {}

PipelinerPtr Pipeliner::transform(TransformFn transform) {
    transforms_.push_back(transform);
    return shared_from_this();
}

void Pipeliner::set_buffer_size(size_t size) {
    buffer_size_ = size;
}

int Pipeliner::sync() {
    if (!produced_) {
        produced_ = true;
        if (!process_pipeline()) {
            setstate(std::ios::failbit);
            return -1;
        }
    }
    return 0;
}

bool Pipeliner::process_pipeline() {
    try {
        std::stringstream current_stream;

        // Execute producer
        if (!producer_(current_stream)) {
            BOOST_LOG_TRIVIAL(error) << "Producer failed in pipeline";
            return false;
        }

        // Apply each transformation
        for (const auto& transform : transforms_) {
            std::stringstream next_stream;
            if (!transform(current_stream, next_stream)) {
                BOOST_LOG_TRIVIAL(error) << "Transform failed in pipeline";
                return false;
            }
            current_stream = std::move(next_stream);
        }

        // Set final result
        str(current_stream.str());
        return true;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Pipeline processing error: " << e.what();
        return false;
    }
}

} // namespace utils
} // namespace dfs
