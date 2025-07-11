//
//  echo_server.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/11.
//

#pragma once

#include <functional>
#include <unordered_map>
#include "listener.hpp"
#include "websocket.hpp"

class EchoSession : public av::net::WSSvrSession {
 public:
  enum Event {
    EventOnOpen = 0,
    EventOnClose,
    EventOnMessage,
    EventOnError,
  };

  using callback_type = std::function<void(EchoSession*, Event, const std::string&)>;

  EchoSession(tcp_socket&& socket, callback_type&& cb);

  virtual ~EchoSession();

  void start();

  void stop();

  void send_msg(const std::string& msg);

 private:
  virtual bool on_handshake_cb() override;

  virtual void on_open_cb() override;

  virtual void on_close_cb() override;

  virtual void on_message_cb(const std::string& msg) override;

  virtual void on_error_cb(const std::exception& e) override;

  callback_type cb_;
};

class EchoServer : public av::net::Listener {
 public:
  static std::shared_ptr<EchoServer>
  createEchoServer(io_context& io, const std::string& ip, const std::string& port);

  EchoServer(io_context& io, tcp_endpoint ep);

  virtual ~EchoServer();

  void start();

 private:
  virtual void on_accept_cb(tcp_socket&& socket) override;

  virtual void on_error_cb(const std::exception& e) override;

  void on_session_event_cb(EchoSession* s, EchoSession::Event ev, const std::string& msg);

  std::unordered_map<EchoSession*, std::shared_ptr<EchoSession>> session_table_;
};
