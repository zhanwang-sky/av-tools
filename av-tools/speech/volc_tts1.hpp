//
//  volc_tts.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/2.
//

#ifndef volc_tts_hpp
#define volc_tts_hpp

#include <functional>
#include "websocket1.hpp"

namespace av {

namespace speech1 {

class VolcTTS : public utils::Websocket {
  struct Message {
    enum MsgType {
      MsgTypeFullClient = (0x1 << 4),
      MsgTypeAudioOnlyClient = (0x2 << 4),
      MsgTypeFullServer = (0x9 << 4),
      MsgTypeAudioOnlyServer = (0xb << 4),
      MsgTypeFrontEndResultServer = (0xc << 4),
      MsgTypeError = (0xf << 4),
    };

    enum MsgFlag {
      MsgFlagNoSeq = 0,
      MsgFlagPositiveSeq = 1,
      MsgFlagLastNoSeq = 2,
      MsgFlagNegativeSeq = 3,
      MsgFlagWithEvent = 4,
    };

    enum Event {
      EventNone = 0,
      EventStartConnection = 1,
      EventFinishConnection = 2,
      EventConnectionStarted = 50,
      EventConnectionFailed = 51,
      EventConnectionFinished = 52,
      EventStartSession = 100,
      EventFinishSession = 102,
      EventSessionStarted = 150,
      EventSessionFinished = 152,
      EventSessionFailed = 153,
      EventTaskRequest = 200,
      EventTTSSentenceStart = 350,
      EventTTSSentenceEnd = 351,
      EventTTSResponse = 352,
    };

    static Message parse(const std::string& data);

    std::string dump();

    MsgType msg_type;
    MsgFlag msg_flag;
    Event event = EventNone;
    uint32_t error_code = 0;
    std::string session_id;
    std::string connect_id;
    std::string payload;
  };

 public:
  enum Event {
    EventConnOpen = 0,
    EventConnClose,
    EventConnError,
    EventTTSStart,
    EventTTSStop,
    EventTTSError,
    EventTTSMessage,
    EventTTSAudio,
  };

  using callback_type = std::function<void(const std::string& connect_id,
                                           const std::string& session_id,
                                           Event event,
                                           const std::string& message)>;

  static constexpr const char* host = "openspeech.bytedance.com";
  static constexpr const char* url = "/api/v3/tts/bidirection";
  static constexpr const char* ns = "BidirectionalTTS";

  VolcTTS(boost::asio::io_context& io, callback_type&& cb,
          const std::string& app_key, const std::string& access_key,
          const std::string& resource_id, const std::string& connect_id,
          const std::string& session_id, const std::string& speaker);

  virtual ~VolcTTS();

  void start();

  void request(const std::string& text);

  void stop(bool force = false);

 private:
  virtual void on_open() override;

  virtual void on_close() override;

  virtual void on_message(const std::string& msg) override;

  virtual void on_error(const std::exception& e) override;

  void on_post_start();

  void on_post_request(std::shared_ptr<std::string> p_text);

  void on_post_stop(bool force);

  void start_connection();

  void start_tts_session();

  void send_tts_message(std::shared_ptr<std::string> p_text);

  void finish_tts_session();

  void finish_connection();

  std::string make_json_payload(Message::Event event,
                                std::shared_ptr<std::string> p_text = nullptr);

  inline void callback(Event ev, const std::string& msg) {
    cb_(connect_id_, session_id_, ev, msg);
  }

  callback_type cb_;
  std::string connect_id_;
  std::string session_id_;
  std::string speaker_;
  int state_ = 0;
  // 0. init
  // 1. opened
  // 2. connectionStarted
  // 3. sessionStarted
  // 4. sessionFinished
  // 5. connectionFinished
  // 6. error
  // 7. closed
};

} // speech

} // av

#endif /* volc_tts_hpp */
