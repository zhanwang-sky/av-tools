//
//  echo_server.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/11.
//

#include <iostream>
#include <sstream>
#include "echo_server.hpp"

using std::cout;
using std::cerr;
using std::endl;

EchoSession::EchoSession(tcp_socket&& socket, callback_type&& cb)
    : av::net::WSSvrSession(std::move(socket)),
      cb_(std::move(cb))
{
  cout << "Session<" << this << "> constructed\n";
}

EchoSession::~EchoSession() {
  cout << "Session<" << this << "> destroyed\n";
}

void EchoSession::start() {
  run();
}

void EchoSession::stop() {
  close();
}

void EchoSession::send_msg(const std::string &msg) {
  send(msg);
}

bool EchoSession::on_handshake_cb() {
  std::ostringstream oss;
  auto& req = get_request_from_cb();
  auto& resp = get_response_from_cb();

  oss << "Session<" << this << "> receive request:\n";
  for (auto& hdr : req) {
    oss << " " << hdr.name_string() << ":" << hdr.value() << endl;
  }
  cout << oss.str();

  resp.set("X-FOO", "BAR");

  return true;
}

void EchoSession::on_open_cb() {
  cb_(this, EventOnOpen, "");
}

void EchoSession::on_close_cb() {
  cb_(this, EventOnClose, "");
}

void EchoSession::on_message_cb(std::string_view msg) {
  cb_(this, EventOnMessage, std::string(msg));
}

void EchoSession::on_error_cb(const std::exception& e) {
  cb_(this, EventOnError, e.what());
}

std::shared_ptr<EchoServer>
EchoServer::createEchoServer(io_context& io,
                             const std::string& ip, const std::string& port) {
  auto addr = boost::asio::ip::make_address(ip);
  auto port_num = static_cast<uint16_t>(std::stoi(port));
  return std::make_shared<EchoServer>(io, tcp_endpoint(addr, port_num));
}

EchoServer::EchoServer(io_context& io, tcp_endpoint ep)
    : av::net::Listener(io, ep)
{
}

EchoServer::~EchoServer() { }

void EchoServer::start() {
  run();
}

void EchoServer::on_accept_cb(tcp_socket&& socket) {
  cout << "EchoServer: new client from "
       << socket.remote_endpoint().address()
       << ":" << socket.remote_endpoint().port()
       << endl;

  auto p_session = std::make_shared<EchoSession>(std::move(socket),
                                                 boost::beast::bind_front_handler(&EchoServer::on_session_event_cb,
                                                                                  shared_from_base<EchoServer>()));
  p_session->start();

  session_table_[p_session.get()] = p_session;
}

void EchoServer::on_error_cb(const std::exception &e) {
  cerr << "EchoServer error: " << e.what() << endl;
  throw e;
}

void EchoServer::on_session_event_cb(EchoSession *s,
                                     EchoSession::Event ev,
                                     const std::string &msg) {
  if (ev == EchoSession::EventOnClose || ev == EchoSession::EventOnError) {
    if (ev == EchoSession::EventOnClose) {
      cout << "Session<" << s << "> closed\n";
    } else {
      cerr << "Session<" << s << "> error: " << msg << endl;
    }
    auto self = shared_from_base<EchoServer>();
    boost::asio::post(get_executor(), [this, self, s]() { session_table_.erase(s); });
  } else if (ev == EchoSession::EventOnOpen) {
    cout << "Session<" << s << "> opened\n";
  } else if (ev == EchoSession::EventOnMessage) {
    s->send_msg(msg);
  } else {
    cerr << "Session<" << s << "> unknown event: " << ev << endl;
  }
}
