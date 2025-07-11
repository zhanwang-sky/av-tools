//
//  websocket.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/9.
//

#ifndef av_net_websocket_hpp
#define av_net_websocket_hpp

#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/system.hpp>

namespace av {

namespace net {

class WSSvrSession : public std::enable_shared_from_this<WSSvrSession> {
 public:
  using tcp_socket = boost::asio::ip::tcp::socket;
  using request_type = boost::beast::websocket::request_type;
  using response_type = boost::beast::websocket::response_type;
  using stream_base = boost::beast::websocket::stream_base;
  using tcp_stream = boost::beast::tcp_stream;
  using ws_stream = boost::beast::websocket::stream<tcp_stream>;

  WSSvrSession(tcp_socket&& socket) : ws_(std::move(socket)) { }

  virtual ~WSSvrSession() { }

 protected:
  template <typename T>
  std::shared_ptr<T> shared_from_base() {
    return std::static_pointer_cast<T>(shared_from_this());
  }

  boost::asio::any_io_executor get_executor() {
    return ws_.get_executor();
  }

  void run() {
    boost::asio::dispatch(ws_.get_executor(),
                          boost::beast::bind_front_handler(&WSSvrSession::on_post_run,
                                                           shared_from_this()));
  }

  void send(const std::string& msg) {
    auto p_msg = std::make_shared<std::string>(msg);
    boost::asio::dispatch(ws_.get_executor(),
                          boost::beast::bind_front_handler(&WSSvrSession::on_post_send,
                                                           shared_from_this(),
                                                           p_msg));
  }

  void close() {
    boost::asio::dispatch(ws_.get_executor(),
                          boost::beast::bind_front_handler(&WSSvrSession::on_post_close,
                                                           shared_from_this()));
  }

  request_type& get_request_from_cb() { return req_; }

  response_type& get_response_from_cb() { return resp_; }

  virtual bool on_handshake_cb() { return true; }

  virtual void on_open_cb() = 0;

  virtual void on_close_cb() = 0;

  virtual void on_message_cb(const std::string& msg) = 0;

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
      msg_queue_.push(p_msg);
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

    ws_.set_option(stream_base::timeout::suggested(boost::beast::role_type::server));

    ws_.set_option(stream_base::decorator([r = this->resp_](response_type& resp) {
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

    open_ = 1;
    on_open_cb();

    async_read();
    async_write();
  }

  void on_disconnect(boost::beast::error_code) {
    close_ = 1;
    on_close_cb();
  }

  void on_read(boost::beast::error_code ec, std::size_t) {
    if (should_exit(ec)) {
      return;
    }

    auto cbuf = buf_.cdata();
    std::string msg(static_cast<const char*>(cbuf.data()), cbuf.size());
    on_message_cb(msg);
    buf_.consume(buf_.size());

    async_read();
  }

  void on_write(boost::beast::error_code ec, std::size_t) {
    if (should_exit(ec)) {
      return;
    }

    msg_queue_.pop();

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
  std::queue<std::shared_ptr<std::string>> msg_queue_;
  int open_ = -1;
  int close_ = -1;
};

} // net

} // av

#endif /* av_net_websocket_hpp */
