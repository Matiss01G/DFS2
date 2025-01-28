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
    static PipelinerPtr create(ProducerFn producer);
    explicit Pipeliner(ProducerFn producer);
    PipelinerPtr transform(TransformFn transform);
    void set_buffer_size(size_t size);

protected:
    // Override basic_stringbuf's sync to implement lazy evaluation
    virtual int sync();

private:
    bool process_pipeline();

    ProducerFn producer_;
    std::vector<TransformFn> transforms_;
    size_t buffer_size_;
    bool produced_;
};

} // namespace utils
} // namespace dfs