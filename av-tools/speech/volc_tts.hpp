//
//  volc_tts.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/16.
//

#pragma once

#include <functional>
#include "websocket.hpp"

namespace av {

namespace speech {

class VolcTTS : public net::WSSCliSession {
  class SSLCtx;
  struct Message;

 public:
  enum Event {
    EventError = 0,
    EventOpen,
    EventClose,
    EventConnStarted,
    EventConnFinished,
    EventSessStarted,
    EventSessFinished,
    EventTTSMessage,
    EventTTSAudio
  };

  using callback_type = std::function<void(Event ev, std::string_view id, std::string_view msg)>;

  static constexpr const char* host = "openspeech.bytedance.com";
  static constexpr const char* url = "/api/v3/tts/bidirection";

  static std::shared_ptr<VolcTTS>
  createVolcTTS(WSSCliSession::io_context& io,
                std::string_view appid, std::string_view token, std::string_view resid,
                callback_type&& cb);

  VolcTTS(WSSCliSession::io_context& io, WSSCliSession::ssl_context& ssl,
          std::string_view appid, std::string_view token, std::string_view resid,
          callback_type&& cb);

  virtual ~VolcTTS();

  void connect();

  void teardown();

  void start_session(std::string_view id, std::string_view speaker);

  void stop_session();

  void request(std::string_view text);

 private:
  void on_post_connect();

  void on_post_teardown();

  void on_post_start(std::shared_ptr<std::string> p_id,
                     std::shared_ptr<std::string> p_speaker);

  void on_post_stop();

  void on_post_request(std::shared_ptr<std::string> p_text);

  void tts_start_connection();

  void tts_start_session();

  void tts_stop_session();

  void tts_send_request(std::shared_ptr<std::string> p_text);

  virtual bool on_handshake_cb() override;

  virtual void on_open_cb() override;

  virtual void on_close_cb() override;

  virtual void on_message_cb(std::string_view msg) override;

  virtual void on_error_cb(const std::exception& e) override;

  std::string appid_;
  std::string token_;
  std::string resid_;
  std::string logid_;
  callback_type cb_;
  std::shared_ptr<std::string> p_sess_id_;
  std::shared_ptr<std::string> p_speaker_;
  int state_ = 0;
  // 0. init
  // 1. connecting    (transient)
  // 2. connected
  // 3. creating      (transient)
  // 4. sessionReady
  // 5. deleting      (transient)
  // 6. disconnecting (transient)
  // 7. closed
};

} // speech

} // av
