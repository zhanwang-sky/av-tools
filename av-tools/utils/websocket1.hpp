//
//  websocket.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/6/10.
//

#ifndef websocket_hpp
#define websocket_hpp

/// https://websocket-client.readthedocs.io/en/latest/app.html

#include <chrono>
#include <map>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

namespace av {

namespace utils {

class Websocket : public std::enable_shared_from_this<Websocket> {
  using io_context = boost::asio::io_context;
  using tcp_resolver = boost::asio::ip::tcp::resolver;
  using ssl_context = boost::asio::ssl::context;
  using tcp_stream = boost::beast::tcp_stream;
  using ssl_stream = boost::asio::ssl::stream<tcp_stream>;
  using wss_stream = boost::beast::websocket::stream<ssl_stream>;

 public:
  Websocket(io_context& io,
            const std::string& host, const std::string& port, const std::string& url,
            const std::map<std::string, std::string>& headers)
      : io_(io),
        strand_(boost::asio::make_strand(io)),
        resolver_(strand_),
        ssl_(ssl_context::tlsv12_client),
        host_(host),
        port_(port),
        url_(url),
        headers_(headers)
  {
    ssl_.set_verify_mode(boost::asio::ssl::verify_peer);
    ssl_.set_default_verify_paths();

    ws_ = std::make_unique<wss_stream>(strand_, ssl_);

    if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host_.c_str())) {
      throw std::runtime_error("SSL: error setting SNI");
    }
    ws_->next_layer().set_verify_callback(boost::asio::ssl::host_name_verification(host_));
  }

  virtual ~Websocket() { }

 protected:
  template <typename T>
  std::shared_ptr<T> shared_from_base() {
    return std::static_pointer_cast<T>(shared_from_this());
  }

  void run() {
    boost::asio::post(strand_,
                      boost::beast::bind_front_handler(&Websocket::on_post_run,
                                                       shared_from_this()));
  }

  void send(const std::string& msg) {
    auto p_msg = std::make_shared<std::string>(msg);
    boost::asio::post(strand_,
                      boost::beast::bind_front_handler(&Websocket::on_post_send,
                                                       shared_from_this(),
                                                       p_msg));
  }

  void close() {
    boost::asio::post(strand_,
                      boost::beast::bind_front_handler(&Websocket::on_post_close,
                                                       shared_from_this()));
  }

  virtual void on_open() = 0;

  virtual void on_close() = 0;

  virtual void on_message(const std::string& msg) = 0;

  virtual void on_error(const std::exception& e) = 0;

  Websocket(const Websocket&) = delete;
  Websocket(Websocket&&) = delete;
  Websocket& operator=(const Websocket&) = delete;
  Websocket& operator=(Websocket&&) = delete;

 private:
  void on_resolve(const boost::system::error_code& ec,
                  const tcp_resolver::results_type& results) {
    if (should_exit(ec)) {
      return;
    }

    auto& lowest_layer = boost::beast::get_lowest_layer(*ws_);
    lowest_layer.expires_after(std::chrono::seconds(30));
    lowest_layer.async_connect(results,
                               boost::beast::bind_front_handler(&Websocket::on_connect,
                                                                shared_from_this()));
  }

  void on_connect(const boost::system::error_code& ec,
                  const tcp_resolver::results_type::endpoint_type&) {
    if (should_exit(ec)) {
      return;
    }

    boost::beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));
    ws_->next_layer().async_handshake(boost::asio::ssl::stream_base::client,
                                      boost::beast::bind_front_handler(&Websocket::on_ssl_handshake,
                                                                       shared_from_this()));
  }

  void on_ssl_handshake(const boost::system::error_code& ec) {
    if (should_exit(ec)) {
      return;
    }

    boost::beast::get_lowest_layer(*ws_).expires_never();

    auto& suggested = boost::beast::websocket::stream_base::timeout::suggested;
    ws_->set_option(suggested(boost::beast::role_type::client));

    ws_->set_option(boost::beast::websocket::stream_base::decorator(
      [this](boost::beast::websocket::request_type& req) {
        for (const auto& h : headers_) {
          req.set(h.first, h.second);
        }
      }));

    auto host_port = host_ + ":" + port_;
    ws_->async_handshake(host_port, url_,
                         boost::beast::bind_front_handler(&Websocket::on_handshake,
                                                          shared_from_this()));
  }

  void on_handshake(const boost::system::error_code& ec) {
    if (should_exit(ec)) {
      return;
    }

    open_ = 1;

    on_open();

    async_read();

    async_write();
  }

  void on_read(const boost::system::error_code& ec, std::size_t) {
    if (should_exit(ec)) {
      return;
    }

    auto cbuf = buffer_.cdata();
    std::string msg(static_cast<const char*>(cbuf.data()), cbuf.size());
    on_message(msg);
    buffer_.consume(buffer_.size());

    async_read();
  }

  void on_write(const boost::system::error_code& ec, std::size_t) {
    if (should_exit(ec)) {
      return;
    }

    send_queue_.pop();
    async_write();
  }

  void on_disconnect(const boost::system::error_code& ec) {
    close_ = 1;

    on_close();
  }

  void on_post_run() {
    if (open_ < 0 && close_ < 0) {
      open_ = 0;
      resolver_.async_resolve(host_, port_,
                              boost::beast::bind_front_handler(&Websocket::on_resolve,
                                                               shared_from_this()));
    }
  }

  void on_post_send(std::shared_ptr<std::string> p_msg) {
    if (open_ >= 0 && close_ < 0) {
      bool idle = send_queue_.empty();
      send_queue_.push(p_msg);
      if (idle) {
        async_write();
      }
    }
  }

  void on_post_close() {
    if (close_ < 0) {
      close_ = 0;
      resolver_.cancel();
      ws_->async_close(boost::beast::websocket::close_code::normal,
                       boost::beast::bind_front_handler(&Websocket::on_disconnect,
                                                        shared_from_this()));
    }
  }

  inline bool should_exit(const boost::system::error_code& ec) {
    if (ec || close_ >= 0) {
      if (ec) {
        if (ec != boost::asio::error::operation_aborted &&
            ec != boost::asio::error::eof &&
            ec != boost::beast::websocket::error::closed) {
          on_error(std::runtime_error(ec.message()));
        }
        on_post_close();
      }
      return true;
    }
    return false;
  }

  inline void async_read() {
    ws_->async_read(buffer_, boost::beast::bind_front_handler(&Websocket::on_read,
                                                              shared_from_this()));
  }

  inline void async_write() {
    if (!send_queue_.empty()) {
      auto p_msg = send_queue_.front();
      ws_->async_write(boost::asio::buffer(*p_msg),
                       boost::beast::bind_front_handler(&Websocket::on_write,
                                                        shared_from_this()));
    }
  }

 protected:
  io_context& io_;
  decltype(boost::asio::make_strand(io_)) strand_;

 private:
  tcp_resolver resolver_;
  ssl_context ssl_;
  std::unique_ptr<wss_stream> ws_;
  boost::beast::flat_buffer buffer_;
  std::queue<std::shared_ptr<std::string>> send_queue_;
  int open_ = -1;
  int close_ = -1;
  // saved args
  const std::string host_;
  const std::string port_;
  const std::string url_;
  const std::map<std::string, std::string> headers_;
};

} // utils

} // av

#endif /* websocket_hpp */
