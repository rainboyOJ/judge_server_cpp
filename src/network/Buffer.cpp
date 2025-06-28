\
#include "network/Buffer.h" // Assuming relative path from src to include
#include <stdexcept> // For std::out_of_range
#include <cstring>   // For memcpy

namespace boxtest {
namespace network {

Buffer::Buffer(size_t initial_capacity)
    : buffer_(initial_capacity), read_index_(0), write_index_(0) {}

void Buffer::append(const char* data, size_t len) {
    ensure_writable_bytes(len);
    std::copy(data, data + len, begin_write());
    write_index_ += len;
}

void Buffer::append(const std::string& str) {
    append(str.data(), str.length());
}

std::string Buffer::retrieve_as_string(size_t len) {
    if (len > readable_bytes()) {
        throw std::out_of_range("Not enough readable bytes to retrieve");
    }
    std::string result(peek(), len);
    consume(len);
    return result;
}

std::vector<char> Buffer::retrieve_as_vector(size_t len) {
    if (len > readable_bytes()) {
        throw std::out_of_range("Not enough readable bytes to retrieve");
    }
    std::vector<char> result(peek(), peek() + len);
    consume(len);
    return result;
}

std::string Buffer::retrieve_all_as_string() {
    return retrieve_as_string(readable_bytes());
}

std::vector<char> Buffer::retrieve_all_as_vector() {
    return retrieve_as_vector(readable_bytes());
}

std::string Buffer::peek_as_string(size_t len) const {
    if (len > readable_bytes()) {
        throw std::out_of_range("Not enough readable bytes to peek");
    }
    return std::string(peek(), len);
}

std::vector<char> Buffer::peek_as_vector(size_t len) const {
     if (len > readable_bytes()) {
        throw std::out_of_range("Not enough readable bytes to peek");
    }
    return std::vector<char>(peek(), peek() + len);
}

void Buffer::consume(size_t len) {
    if (len > readable_bytes()) {
        read_index_ = write_index_ = 0; // Consume all
    } else {
        read_index_ += len;
    }
    if (read_index_ == write_index_) { // Optimization: reset indices if buffer is empty
        read_index_ = 0;
        write_index_ = 0;
    }
}

size_t Buffer::readable_bytes() const {
    return write_index_ - read_index_;
}

const char* Buffer::peek() const {
    return begin() + read_index_;
}

char* Buffer::begin_write() {
    return begin() + write_index_;
}


void Buffer::ensure_writable_bytes(size_t len) {
    if (buffer_.size() - write_index_ < len) { // Not enough space at the end
        if (read_index_ + (buffer_.size() - write_index_) >= len) {
            // Compact the buffer: move readable data to the front
            size_t readable = readable_bytes();
            std::copy(begin() + read_index_, begin() + write_index_, begin());
            read_index_ = 0;
            write_index_ = readable;
        } else {
            // Resize the buffer
            buffer_.resize(write_index_ + len);
        }
    }
}

void Buffer::swap(Buffer& rhs) noexcept {
    buffer_.swap(rhs.buffer_);
    std::swap(read_index_, rhs.read_index_);
    std::swap(write_index_, rhs.write_index_);
}


char* Buffer::begin() {
    return &*buffer_.begin();
}

const char* Buffer::begin() const {
    return &*buffer_.begin();
}

} // namespace network
} // namespace boxtest
