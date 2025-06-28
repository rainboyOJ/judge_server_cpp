\
#pragma once

#include <vector>
#include <string>
#include <algorithm> // For std::copy, std::min
#include <cstddef>   // For size_t

namespace boxtest {
namespace network {

class Buffer {
public:
    Buffer(size_t initial_capacity = 1024);

    // Appends data to the buffer
    void append(const char* data, size_t len);
    void append(const std::string& str);

    // Retrieves data from the buffer
    std::string retrieve_as_string(size_t len);
    std::vector<char> retrieve_as_vector(size_t len);
    std::string retrieve_all_as_string();
    std::vector<char> retrieve_all_as_vector();

    // Peeks at data without removing it
    std::string peek_as_string(size_t len) const;
    std::vector<char> peek_as_vector(size_t len) const;


    // Consumes (removes) data from the beginning of the buffer
    void consume(size_t len);

    // Returns the number of readable bytes
    size_t readable_bytes() const;

    // Returns a pointer to the beginning of the readable data
    const char* peek() const;
    char* begin_write();


    // Ensures the buffer has enough capacity
    void ensure_writable_bytes(size_t len);
    
    void swap(Buffer& rhs) noexcept;


private:
    std::vector<char> buffer_;
    size_t read_index_;
    size_t write_index_;

    char* begin();
    const char* begin() const;
};

} // namespace network
} // namespace boxtest
