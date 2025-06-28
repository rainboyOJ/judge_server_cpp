
#pragma once

#include "network/Buffer.h"
#include "common/Result.h"
#include <functional>
#include <memory>
#include <string>
#include <asio.hpp> // Using Asio for networking

namespace boxtest {
namespace network {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;
    using OnMessageCallback = std::function<void(Ptr, Buffer&)>;
    using OnCloseCallback = std::function<void(Ptr)>;
    using OnErrorCallback = std::function<void(Ptr, const asio::error_code&)>;

    Connection(asio::ip::tcp::socket socket, size_t id);
    ~Connection();

    void start();
    void send(const std::string& message);
    void send(const char* data, size_t len);
    void close();

    void set_on_message_callback(OnMessageCallback cb) { on_message_cb_ = std::move(cb); }
    void set_on_close_callback(OnCloseCallback cb) { on_close_cb_ = std::move(cb); }
    void set_on_error_callback(OnErrorCallback cb) { on_error_cb_ = std::move(cb); }

    asio::ip::tcp::socket& socket() { return socket_; }
    const asio::ip::tcp::socket& socket() const { return socket_; }
    size_t id() const { return id_; }
    bool is_open() const { return socket_.is_open(); }

private:
    void do_read();
    void do_write();

    asio::ip::tcp::socket socket_;
    size_t id_;
    Buffer read_buffer_;
    Buffer write_buffer_; // Or a queue of messages to send
    bool writing_ = false; // To manage concurrent writes

    OnMessageCallback on_message_cb_;
    OnCloseCallback on_close_cb_;
    OnErrorCallback on_error_cb_;
};

} // namespace network
} // namespace boxtest
