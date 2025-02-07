#pragma once

#include <functional>
#include <memory>
#include <sstream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <boost/log/trivial.hpp>

namespace dfs {
namespace utils {

// Forward declaration
class Pipeliner;

// Type aliases for clarity
using ProducerFn = std::function<bool(std::stringstream&)>;
using TransformFn = std::function<bool(std::stringstream&, std::stringstream&)>;
using PipelinerPtr = std::shared_ptr<Pipeliner>;

class Pipeliner : public std::stringstream, 
         public std::enable_shared_from_this<Pipeliner> {
public:

  // ---- CONSTRUCTOR AND DESTRUCTOR ----
  explicit Pipeliner(ProducerFn producer);


  // ---- PIPELINE CONSTRUCTION METHODS ----
  // Creates pipeline with a producer function
  static PipelinerPtr create(ProducerFn producer);
  // Sets the transformer function used in method chaining
  PipelinerPtr transform(TransformFn transform);


  // ---- PIPELINE EXECUTION AND CONTROL METHODS ----
  // calls sync() to process remaining bytes in pipeline
  void flush();


  // ---- GETTERS AND SETTERS ----
  std::size_t get_total_size() const { return total_size_; }

  void set_buffer_size(size_t size);
  void set_total_size(std::size_t size) { total_size_ = size; }

protected:
  // ---- PIPELINE EXECUTION AND CONTROL METHODS ----
  // Executes pipeline once
  virtual int sync();

private:
  // ---- PARAMETERS ----
  ProducerFn producer_;
  std::vector<TransformFn> transforms_;
  size_t buffer_size_;
  bool produced_;
  bool eof_;
  std::stringstream buffer_;
  std::size_t total_size_{0}; 

  
  // ---- PIPELINE EXECUTION AND CONTROL METHODS ----
  // Processes chunks until buffer reaches target size or EOF
  bool process_pipeline();
  // Gets next chunk from producer, applies transform, 
  // and writes to buffer
  bool process_next_chunk();

};

} // namespace utils
} // namespace dfs