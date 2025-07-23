//
//  websocket.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/9.
//

#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/system.hpp>

namespace av {

namespace utils {

class WSSCliSession : public std::enable_shared_from_this<WSSCliSession> {
  using tcp_stream = boost::beast::tcp_stream;
  using ssl_stream = boost::asio::ssl::stream<tcp_stream>;
  using wss_stream = boost::beast::websocket::stream<ssl_stream>;
  using ws_stream_base = boost::beast::websocket::stream_base;

 public:
  using io_context = boost::asio::io_context;
  using ssl_context = boost::asio::ssl::context;
  using resolver = boost::asio::ip::tcp::resolver;
  using request_type = boost::beast::websocket::request_type;
  using response_type = boost::beast::websocket::response_type;

  WSSCliSession(io_context& io, ssl_context& ssl,
                std::string_view host, std::string_view port,
                std::string_view url)
      : resolver_(boost::asio::make_strand(io)),
        ws_(resolver_.get_executor(), ssl),
        host_(host),
        port_(port),
        url_(url)
  {
    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
      throw std::runtime_error("SSL: error setting SNI");
    }

    ws_.next_layer().set_verify_callback(boost::asio::ssl::host_name_verification(host_));
  }

  virtual ~WSSCliSession() { }

 protected:
  template <typename T>
  std::shared_ptr<T> shared_from_base() {
    return std::static_pointer_cast<T>(shared_from_this());
  }

  boost::asio::any_io_executor get_executor() {
    return ws_.get_executor();
  }

  request_type& get_request_from_cb() { return req_; }

  response_type& get_response_from_cb() { return resp_; }

  virtual void run() {
    boost::asio::post(ws_.get_executor(),
                      boost::beast::bind_front_handler(&WSSCliSession::on_post_run,
                                                       shared_from_this()));
  }

  virtual void send(std::string_view msg) {
    auto p_msg = std::make_shared<std::string>(msg);
    boost::asio::post(ws_.get_executor(),
                      boost::beast::bind_front_handler(&WSSCliSession::on_post_send,
                                                       shared_from_this(),
                                                       p_msg));
  }

  virtual void close() {
    boost::asio::post(ws_.get_executor(),
                      boost::beast::bind_front_handler(&WSSCliSession::on_post_close,
                                                       shared_from_this()));
  }

  virtual bool on_handshake_cb() { return true; }

  virtual void on_open_cb() = 0;

  virtual void on_close_cb() = 0;

  virtual void on_message_cb(std::string_view msg) = 0;

  virtual void on_error_cb(const std::exception& e) = 0;

 private:
  void on_post_run() {
    if (open_ < 0 && close_ < 0) {
      open_ = 0;
      resolver_.async_resolve(host_, port_,
                              boost::beast::bind_front_handler(&WSSCliSession::on_resolve,
                                                               shared_from_this()));
    }
  }

  void on_post_send(std::shared_ptr<std::string> p_msg) {
    if (open_ >= 0 && close_ < 0) {
      bool idle = (open_ > 0) && msg_queue_.empty();
      msg_queue_.push_back(p_msg);
      if (idle) {
        async_write();
      }
    }
  }

  void on_post_close() {
    if (close_ < 0) {
      close_ = 0;
      resolver_.cancel();
      ws_.async_close(boost::beast::websocket::close_code::normal,
                      boost::beast::bind_front_handler(&WSSCliSession::on_disconnect,
                                                       shared_from_this()));
    }
  }

  void on_resolve(boost::system::error_code ec,
                  resolver::results_type results) {
    if (should_exit(ec)) {
      return;
    }

    auto& lowest_layer = boost::beast::get_lowest_layer(ws_);
    lowest_layer.expires_after(std::chrono::seconds(30));
    lowest_layer.async_connect(results,
                               boost::beast::bind_front_handler(&WSSCliSession::on_connect,
                                                                shared_from_this()));
  }

  void on_connect(boost::system::error_code ec,
                  resolver::results_type::endpoint_type) {
    if (should_exit(ec)) {
      return;
    }

    boost::beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
    ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::client,
                                     boost::beast::bind_front_handler(&WSSCliSession::on_ssl_handshake,
                                                                      shared_from_this()));
  }

  void on_ssl_handshake(boost::system::error_code ec) {
    if (should_exit(ec)) {
      return;
    }

    boost::beast::get_lowest_layer(ws_).expires_never();

    if (!on_handshake_cb()) {
      on_post_close();
      return;
    }

    ws_.set_option(ws_stream_base::timeout::suggested(boost::beast::role_type::client));
    ws_.set_option(ws_stream_base::decorator([r = this->req_](request_type& req) {
      for (const auto& header : r) {
        if (header.name() == boost::beast::http::field::unknown) {
          req.set(header.name_string(), header.value());
        }
      }
    }));
    ws_.async_handshake(resp_, host_ + ":" + port_, url_,
                        boost::beast::bind_front_handler(&WSSCliSession::on_handshake,
                                                         shared_from_this()));
  }

  void on_handshake(boost::beast::error_code ec) {
    if (should_exit(ec)) {
      return;
    }

    on_open_cb();

    open_ = 1;
    async_read();
    async_write();
  }

  void on_read(boost::beast::error_code ec, std::size_t) {
    if (should_exit(ec)) {
      return;
    }

    auto cbuf = buf_.cdata();
    std::string_view msg(static_cast<const char*>(cbuf.data()), cbuf.size());
    on_message_cb(msg);
    buf_.consume(buf_.size());

    async_read();
  }

  void on_write(boost::beast::error_code ec, std::size_t) {
    if (should_exit(ec)) {
      return;
    }

    msg_queue_.pop_front();

    async_write();
  }

  void on_disconnect(boost::beast::error_code) {
    on_close_cb();
    close_ = 1;
  }

  inline void async_read() {
    ws_.async_read(buf_,
                   boost::beast::bind_front_handler(&WSSCliSession::on_read,
                                                    shared_from_this()));
  }

  inline void async_write() {
    if (!msg_queue_.empty()) {
      auto& p_msg = msg_queue_.front();
      ws_.async_write(boost::asio::buffer(*p_msg),
                      boost::beast::bind_front_handler(&WSSCliSession::on_write,
                                                       shared_from_this()));
    }
  }

  inline bool should_exit(const boost::system::error_code& ec) {
    if (ec || close_ >= 0) {
      if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
          on_error_cb(std::runtime_error(ec.message()));
        }
        on_post_close();
      }
      return true;
    }
    return false;
  }

  resolver resolver_;
  wss_stream ws_;
  request_type req_;
  response_type resp_;
  boost::beast::flat_buffer buf_;
  std::list<std::shared_ptr<std::string>> msg_queue_;
  std::string host_;
  std::string port_;
  std::string url_;
  int open_ = -1;
  int close_ = -1;
};

class WSSvrSession : public std::enable_shared_from_this<WSSvrSession> {
  using tcp_stream = boost::beast::tcp_stream;
  using ws_stream = boost::beast::websocket::stream<tcp_stream>;
  using ws_stream_base = boost::beast::websocket::stream_base;

 public:
  using socket = boost::asio::ip::tcp::socket;
  using request_type = boost::beast::websocket::request_type;
  using response_type = boost::beast::websocket::response_type;

  WSSvrSession(socket&& s) : ws_(std::move(s)) { }

  virtual ~WSSvrSession() { }

 protected:
  template <typename T>
  std::shared_ptr<T> shared_from_base() {
    return std::static_pointer_cast<T>(shared_from_this());
  }

  boost::asio::any_io_executor get_executor() {
    return ws_.get_executor();
  }

  request_type& get_request_from_cb() { return req_; }

  response_type& get_response_from_cb() { return resp_; }

  virtual void run() {
    boost::asio::post(ws_.get_executor(),
                      boost::beast::bind_front_handler(&WSSvrSession::on_post_run,
                                                       shared_from_this()));
  }

  virtual void send(std::string_view msg) {
    auto p_msg = std::make_shared<std::string>(msg);
    boost::asio::post(ws_.get_executor(),
                      boost::beast::bind_front_handler(&WSSvrSession::on_post_send,
                                                       shared_from_this(),
                                                       p_msg));
  }

  virtual void close() {
    boost::asio::post(ws_.get_executor(),
                      boost::beast::bind_front_handler(&WSSvrSession::on_post_close,
                                                       shared_from_this()));
  }

  virtual bool on_handshake_cb() { return true; }

  virtual void on_open_cb() = 0;

  virtual void on_close_cb() = 0;

  virtual void on_message_cb(std::string_view msg) = 0;

  virtual void on_error_cb(const std::exception& e) = 0;

 private:
  void on_post_run() {
    if (open_ < 0 && close_ < 0) {
      open_ = 0;
      boost::beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
      boost::beast::http::async_read(ws_.next_layer(), buf_, req_,
                                     boost::beast::bind_front_handler(&WSSvrSession::on_http_request,
                                                                      shared_from_this()));
    }
  }

  void on_post_send(std::shared_ptr<std::string> p_msg) {
    if (open_ >= 0 && close_ < 0) {
      bool idle = (open_ > 0) && msg_queue_.empty();
      msg_queue_.push_back(p_msg);
      if (idle) {
        async_write();
      }
    }
  }

  void on_post_close() {
    if (close_ < 0) {
      close_ = 0;
      ws_.async_close(boost::beast::websocket::close_code::normal,
                      boost::beast::bind_front_handler(&WSSvrSession::on_disconnect,
                                                       shared_from_this()));
    }
  }

  void on_http_request(boost::beast::error_code ec, std::size_t) {
    if (should_exit(ec)) {
      return;
    }

    boost::beast::get_lowest_layer(ws_).expires_never();

    if (!on_handshake_cb()) {
      on_post_close();
      return;
    }

    ws_.set_option(ws_stream_base::timeout::suggested(boost::beast::role_type::server));
    ws_.set_option(ws_stream_base::decorator([r = this->resp_](response_type& resp) {
      for (const auto& header : r) {
        if (header.name() == boost::beast::http::field::unknown) {
          resp.set(header.name_string(), header.value());
        }
      }
    }));
    ws_.async_accept(req_,
                     boost::beast::bind_front_handler(&WSSvrSession::on_accept,
                                                      shared_from_this()));
  }

  void on_accept(boost::beast::error_code ec) {
    if (should_exit(ec)) {
      return;
    }

    on_open_cb();

    open_ = 1;
    async_read();
    async_write();
  }

  void on_disconnect(boost::beast::error_code) {
    on_close_cb();
    close_ = 1;
  }

  void on_read(boost::beast::error_code ec, std::size_t) {
    if (should_exit(ec)) {
      return;
    }

    auto cbuf = buf_.cdata();
    std::string_view msg(static_cast<const char*>(cbuf.data()), cbuf.size());
    on_message_cb(msg);
    buf_.consume(buf_.size());

    async_read();
  }

  void on_write(boost::beast::error_code ec, std::size_t) {
    if (should_exit(ec)) {
      return;
    }

    msg_queue_.pop_front();

    async_write();
  }

  inline void async_read() {
    ws_.async_read(buf_,
                   boost::beast::bind_front_handler(&WSSvrSession::on_read,
                                                    shared_from_this()));
  }

  inline void async_write() {
    if (!msg_queue_.empty()) {
      auto& p_msg = msg_queue_.front();
      ws_.async_write(boost::asio::buffer(*p_msg),
                      boost::beast::bind_front_handler(&WSSvrSession::on_write,
                                                       shared_from_this()));
    }
  }

  inline bool should_exit(const boost::system::error_code& ec) {
    if (ec || close_ >= 0) {
      if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
          on_error_cb(std::runtime_error(ec.message()));
        }
        on_post_close();
      }
      return true;
    }
    return false;
  }

  ws_stream ws_;
  boost::beast::flat_buffer buf_;
  request_type req_;
  response_type resp_;
  std::list<std::shared_ptr<std::string>> msg_queue_;
  int open_ = -1;
  int close_ = -1;
};

} // utils

} // av
