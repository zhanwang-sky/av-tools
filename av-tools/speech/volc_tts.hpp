//
//  volc_tts.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/16.
//

#pragma once

#include "websocket.hpp"
#include "speech_concept.hpp"

namespace av {

namespace speech {

class VolcTTS : public utils::WSSCliSession {
  class SSLCtx;
  struct Message;

 public:
  static constexpr const char* host = "openspeech.bytedance.com";
  static constexpr const char* url = "/api/v3/tts/bidirection";
  static constexpr const char* default_speaker = "zh_female_shuangkuaisisi_moon_bigtts";

  static std::shared_ptr<VolcTTS>
  createVolcTTS(WSSCliSession::io_context& io,
                std::string_view appid, std::string_view token, std::string_view resid,
                SpeechCallback&& cb);

  VolcTTS(WSSCliSession::io_context& io, WSSCliSession::ssl_context& ssl,
          std::string_view appid, std::string_view token, std::string_view resid,
          SpeechCallback&& cb);

  virtual ~VolcTTS();

  virtual void run() override;

  virtual void close() override;

  void request(const TTSRequest& req);

  void request(TTSRequest&& req);

 private:
  void on_post_run();

  void on_post_close();

  void on_post_request(std::shared_ptr<TTSRequest> p_req);

  void process_next();

  void tts_start_connection();

  void tts_start_session(std::string_view session, std::string_view speaker);

  void tts_stop_session();

  void tts_send_request(std::string_view text);

  virtual bool on_handshake_cb() override;

  virtual void on_open_cb() override;

  virtual void on_close_cb() override;

  virtual void on_message_cb(std::string_view msg) override;

  virtual void on_error_cb(const std::exception& e) override;

  std::string appid_;
  std::string token_;
  std::string resid_;
  std::string logid_;
  std::string connid_;
  SpeechCallback cb_;
  std::list<std::shared_ptr<TTSRequest>> req_list_;
  std::string curr_session_;
  std::string curr_speaker_;
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
