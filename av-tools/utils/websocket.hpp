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
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

namespace utils {

class Websocket {
  using io_context = boost::asio::io_context;
  using ssl_context = boost::asio::ssl::context;
  using tcp_resolver = boost::asio::ip::tcp::resolver;
  using tcp_stream = boost::beast::tcp_stream;
  using ssl_stream = boost::asio::ssl::stream<tcp_stream>;
  using wss_stream = boost::beast::websocket::stream<ssl_stream>;

 public:
  Websocket(io_context& io,
            std::string_view host, std::string_view port, std::string_view url,
            const std::map<std::string, std::string>& headers)
      : io_(io),
        resolver_(boost::asio::make_strand(io)),
        ssl_(ssl_context::tlsv12_client),
        host_(host),
        port_(port),
        url_(url),
        headers_(headers)
  {
    ssl_.set_verify_mode(boost::asio::ssl::verify_peer);
    ssl_.set_default_verify_paths();

    ws_ = std::make_unique<wss_stream>(boost::asio::make_strand(io_), ssl_);

    if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host_.c_str())) {
      throw std::runtime_error("SSL: error setting SNI");
    }
    ws_->next_layer().set_verify_callback(boost::asio::ssl::host_name_verification(host_));
  }

  virtual ~Websocket() { }

  void run() {
    resolver_.async_resolve(host_, port_,
                            std::bind(&Websocket::on_resolve,
                                      this,
                                      std::placeholders::_1,
                                      std::placeholders::_2));
  }

  void send(std::string_view msg) {
    ws_->async_write(boost::asio::buffer(msg),
                     std::bind(&Websocket::on_write,
                               this,
                               std::placeholders::_1));
  }

  void close() {
    ws_->async_close(boost::beast::websocket::close_code::normal,
                     std::bind(&Websocket::on_disconnect,
                               this,
                               std::placeholders::_1));
  }

  virtual void on_open() = 0;

  virtual void on_close() = 0;

  virtual void on_message(std::string_view msg) = 0;

  virtual void on_error(const std::exception& e) = 0;

  Websocket(const Websocket&) = delete;
  Websocket(Websocket&&) = delete;
  Websocket& operator=(const Websocket&) = delete;
  Websocket& operator=(Websocket&&) = delete;

 private:
  void on_resolve(const boost::system::error_code& error,
                  tcp_resolver::results_type results) {
    if (error) {
      on_error(std::runtime_error(error.message()));
      return;
    }

    auto& lowest_layer = boost::beast::get_lowest_layer(*ws_);
    lowest_layer.expires_after(std::chrono::seconds(30));
    lowest_layer.async_connect(results,
                               std::bind(&Websocket::on_connect,
                                         this,
                                         std::placeholders::_1));
  }

  void on_connect(const boost::system::error_code& error) {
    if (error) {
      on_error(std::runtime_error(error.message()));
      return;
    }

    boost::beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));
    ws_->next_layer().async_handshake(boost::asio::ssl::stream_base::client,
                                      std::bind(&Websocket::on_ssl_handshake,
                                                this,
                                                std::placeholders::_1));
  }

  void on_ssl_handshake(const boost::system::error_code& error) {
    if (error) {
      on_error(std::runtime_error(error.message()));
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
                         std::bind(&Websocket::on_handshake,
                                   this,
                                   std::placeholders::_1));
  }

  void on_handshake(const boost::beast::error_code& ec) {
    if (ec) {
      on_error(std::runtime_error(ec.message()));
      return;
    }

    on_open();

    async_read();
  }

  void on_read(const boost::beast::error_code& ec, std::size_t/* nbytes*/) {
    if (ec) {
      on_error(std::runtime_error(ec.message()));
      return;
    }

    auto cdata = buffer_.cdata();
    std::string_view msg(static_cast<const char*>(cdata.data()), cdata.size());
    on_message(msg);
    buffer_.consume(buffer_.size());

    async_read();
  }

  void on_write(const boost::beast::error_code& ec) {
    if (ec) {
      on_error(std::runtime_error(ec.message()));
    }
  }

  void on_disconnect(const boost::beast::error_code& ec) {
    if (ec) {
      on_error(std::runtime_error(ec.message()));
      return;
    }

    on_close();
  }

  inline void async_read() {
    ws_->async_read(buffer_, std::bind(&Websocket::on_read,
                                       this,
                                       std::placeholders::_1,
                                       std::placeholders::_2));
  }

  io_context& io_;
  tcp_resolver resolver_;
  ssl_context ssl_;
  std::unique_ptr<wss_stream> ws_;
  boost::beast::flat_buffer buffer_;
  // saved args
  const std::string host_;
  const std::string port_;
  const std::string url_;
  const std::map<std::string, std::string> headers_;
};

} // utils

#endif /* websocket_hpp */
