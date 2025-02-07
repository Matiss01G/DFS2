#include "utils/pipeliner.hpp"

namespace dfs {
namespace utils {

//==============================================
// CONSTRUCTOR AND DESTRUCTOR
//==============================================

Pipeliner::Pipeliner(ProducerFn producer)
  : producer_(producer)
  , buffer_size_(8192) 
  , produced_(false)
  , eof_(false) {}

  
//==============================================
// PIPELINE CONSTRUCTION METHODS 
//==============================================
  
PipelinerPtr Pipeliner::create(ProducerFn producer) {
  return std::make_shared<Pipeliner>(producer);
}
  
PipelinerPtr Pipeliner::transform(TransformFn transform) {
  transforms_.push_back(transform);
  return shared_from_this();
}

  
//==============================================
// PIPELINE EXECUTION AND CONTROL METHODS 
//==============================================

void Pipeliner::flush() {
  if (sync() != 0) { 
    BOOST_LOG_TRIVIAL(error) << "Flush failed"; 
  }
}

int Pipeliner::sync() {
  // Only process data once
  if (!produced_) {
    produced_ = true;
    if (!process_pipeline()) {
      setstate(std::ios::failbit);
      return -1;
    }
  }
  return 0;
}

bool Pipeliner::process_next_chunk() {
  try {
    // Clear current chunk buffer
    std::stringstream chunk;
    chunk.str("");

    // Get next chunk from producer
    if (!producer_(chunk)) {
      eof_ = true;
      return false;
    }

    // Check if we got any data
    std::string chunk_data = chunk.str();
    if (chunk_data.empty()) {
      eof_ = true;
      return false;
    }

    // Process chunk through transforms
    std::stringstream current_stream(chunk_data);

    for (const auto& transform : transforms_) {
      std::stringstream next_stream;
      if (!transform(current_stream, next_stream)) {
        BOOST_LOG_TRIVIAL(error) << "Transform failed in pipeline";
        return false;
      }
      current_stream = std::move(next_stream);
    }

    // Append transformed chunk to buffer
    buffer_ << current_stream.rdbuf();
    return true;

  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Chunk processing error: " << e.what();
    return false;
  }
}

bool Pipeliner::process_pipeline() {
  try {
    // Process chunks until we have enough data
    while (buffer_.tellp() < static_cast<std::streampos>(buffer_size_) && !eof_) {
      if (!process_next_chunk() && !eof_) {
        return false;
      }
    }

    // Set the processed chunks as our content
    str(buffer_.str());
    buffer_.str("");
    return true;

  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Pipeline processing error: " << e.what();
    return false;
  }
}

  
//==============================================
// GETTERS AND SETTERS
//==============================================
  
void Pipeliner::set_buffer_size(size_t size) {
  buffer_size_ = size;
}


  
} // namespace utils
} // namespace dfs