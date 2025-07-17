//
//  volc_tts.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/16.
//

#include "volc_tts.hpp"

using namespace av::speech;

class VolcTTS::SSLCtx {
 public:
  static WSSCliSession::ssl_context& get() {
    static SSLCtx ctx;
    return ctx.ssl_;
  }

  ~SSLCtx() = default;

 private:
  SSLCtx() : ssl_(WSSCliSession::ssl_context::tlsv13_client) {
    ssl_.set_verify_mode(boost::asio::ssl::verify_peer);
    ssl_.set_default_verify_paths();
  }

  WSSCliSession::ssl_context ssl_;
};

std::shared_ptr<VolcTTS>
VolcTTS::createVolcTTS(WSSCliSession::io_context& io,
                       std::string_view appid, std::string_view token, std::string_view resid,
                       callback_type&& cb) {
  return std::make_shared<VolcTTS>(io, SSLCtx::get(), appid, token, resid, std::move(cb));
}

VolcTTS::VolcTTS(WSSCliSession::io_context& io, WSSCliSession::ssl_context& ssl,
                 std::string_view appid, std::string_view token, std::string_view resid,
                 callback_type&& cb)
    : net::WSSCliSession(io, ssl, host, "443", url),
      appid_(appid),
      token_(token),
      resid_(resid),
      cb_(std::move(cb))
{
}

VolcTTS::~VolcTTS() { }

void VolcTTS::connect() {
  boost::asio::post(get_executor(),
                    boost::beast::bind_front_handler(&VolcTTS::on_post_connect,
                                                     shared_from_base<VolcTTS>()));
}

void VolcTTS::teardown() {
  boost::asio::post(get_executor(),
                    boost::beast::bind_front_handler(&VolcTTS::on_post_teardown,
                                                     shared_from_base<VolcTTS>()));
}

void VolcTTS::start_session(std::string_view session_id) {
  auto p_id = std::make_shared<std::string>(session_id);
  boost::asio::post(get_executor(),
                    boost::beast::bind_front_handler(&VolcTTS::on_post_start,
                                                     shared_from_base<VolcTTS>(),
                                                     p_id));
}

void VolcTTS::stop_session() {
  boost::asio::post(get_executor(),
                    boost::beast::bind_front_handler(&VolcTTS::on_post_stop,
                                                     shared_from_base<VolcTTS>()));
}

void VolcTTS::request(std::string_view text) {
  auto p_text = std::make_shared<std::string>(text);
  boost::asio::post(get_executor(),
                    boost::beast::bind_front_handler(&VolcTTS::on_post_request,
                                                     shared_from_base<VolcTTS>(),
                                                     p_text));
}

void VolcTTS::on_post_connect() {
  if (state_ <= 0) {
    run();
  }
}

void VolcTTS::on_post_teardown() {
  if (state_ < 6) {
    state_ = 6;
    close();
  }
}

void VolcTTS::on_post_start(std::shared_ptr<std::string> p_id) {
  if (state_ == 2) {
    state_ = 3;
    // XXX TODO: createSession
  }
}

void VolcTTS::on_post_stop() {
  if (state_ == 4) {
    state_ = 5;
    // XXX TODO: deleteSession
  }
}

void VolcTTS::on_post_request(std::shared_ptr<std::string> p_text) {
  if (state_ == 4) {
    // XXX TODO: taskRequest
  }
}

bool VolcTTS::on_handshake_cb() {
  auto& req = get_request_from_cb();
  req.set("X-Api-App-Key", appid_);
  req.set("X-Api-Access-Key", token_);
  req.set("X-Api-Resource-Id", resid_);
  return true;
}

void VolcTTS::on_open_cb() {
  auto& resp = get_response_from_cb();
  if (resp.find("X-Tt-Logid") != resp.end()) {
    logid_ = resp["X-Tt-Logid"];
  }
  cb_(EventOpen, logid_, "");

  state_ = 1;
  // XXX TODO: startConnection
}

void VolcTTS::on_close_cb() {
  cb_(EventClose, logid_, "");

  state_ = 7;
}

void VolcTTS::on_message_cb(std::string_view msg) {
  // XXX TODO: parse message
}

void VolcTTS::on_error_cb(const std::exception& e) {
  cb_(EventError, logid_, e.what());

  if (state_ < 6) {
    state_ = 6;
  }
}
