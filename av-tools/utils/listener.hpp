//
//  listener.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/9.
//

#pragma once

#include <memory>
#include <stdexcept>
#include <utility>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/system.hpp>

namespace av {

namespace utils {

class Listener : public std::enable_shared_from_this<Listener> {
  using socket_base = boost::asio::socket_base;

 public:
  using io_context = boost::asio::io_context;
  using acceptor = boost::asio::ip::tcp::acceptor;
  using endpoint = boost::asio::ip::tcp::endpoint;
  using socket = boost::asio::ip::tcp::socket;

  Listener(io_context& io, endpoint ep)
      : io_(io),
        acceptor_(boost::asio::make_strand(io))
  {
    acceptor_.open(ep.protocol());

    acceptor_.set_option(socket_base::reuse_address(true));

    acceptor_.bind(ep);

    acceptor_.listen(socket_base::max_listen_connections);
  }

  virtual ~Listener() { }

 protected:
  template <typename T>
  std::shared_ptr<T> shared_from_base() {
    return std::static_pointer_cast<T>(shared_from_this());
  }

  boost::asio::any_io_executor get_executor() {
    return acceptor_.get_executor();
  }

  virtual void run() {
    async_accept();
  }

  virtual void on_accept_cb(socket&& s) = 0;

  virtual void on_error_cb(const std::exception& e) = 0;

 private:
  void on_accept(boost::system::error_code ec, socket s) {
    if (ec) {
      on_error_cb(std::runtime_error(ec.message()));
      return;
    }

    on_accept_cb(std::move(s));

    async_accept();
  }

  inline void async_accept() {
    acceptor_.async_accept(boost::asio::make_strand(io_),
                           boost::beast::bind_front_handler(&Listener::on_accept,
                                                            shared_from_this()));
  }

  io_context& io_;
  acceptor acceptor_;
};

} // utils

} // av
