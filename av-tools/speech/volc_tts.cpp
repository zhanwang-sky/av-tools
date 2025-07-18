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

struct VolcTTS::Message {
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

  template <typename T>
  static inline std::size_t writeBigEndian(std::string& data, T val) {
    for (std::size_t i = sizeof(T); i-- != 0; ) {
      data += static_cast<char>((val >> (8 * i)) & 0xff);
    }
    return sizeof(T);
  }

  template <typename T>
  static inline std::size_t readBigEndian(std::string_view data, T& val) {
    if (data.length() >= sizeof(T)) {
      val = 0;
      for (std::size_t i = 0; i != sizeof(T); ++i) {
        val <<= 8;
        val |= static_cast<uint8_t>(data[i]);
      }
      return sizeof(T);
    }
    return 0;
  }

  template <typename T>
  static inline std::size_t writeString(std::string& data, std::string_view val) {
    auto len = static_cast<T>(val.length());
    writeBigEndian(data, len);
    data += val;
    return sizeof(T) + len;
  }

  template <typename T>
  static inline std::size_t readString(std::string_view data, std::string& val) {
    T len = 0;
    if (!readBigEndian(data, len) || (data.length() < sizeof(T) + len)) {
      return 0;
    }
    val = std::string(data.data() + sizeof(T), len);
    return sizeof(T) + len;
  }

  static Message parse(std::string_view data) {
    Message msg;
    std::size_t read_size = 0;
    std::size_t sz = 0;

    // header
    if (data.length() < 4) {
      throw std::runtime_error("no enough Header bytes");
    }
    msg.msg_type = static_cast<MsgType>(data[1] & 0xf0);
    msg.msg_flag = static_cast<MsgFlag>(data[1] & 0x0f);
    read_size += 4;

    // error code
    if (msg.msg_type == MsgTypeError) {
      auto& ec = msg.error_code;
      sz = readBigEndian(data.substr(read_size), ec);
      if (!sz) {
        throw std::runtime_error("no enough ErrorCode bytes");
      }
      read_size += sz;
    }

    // event
    if (msg.msg_flag == MsgFlagWithEvent) {
      uint32_t ev = 0;
      sz = readBigEndian(data.substr(read_size), ev);
      if (!sz) {
        throw std::runtime_error("no enough Event bytes");
      }
      read_size += sz;
      msg.event = static_cast<Event>(ev);

      // session id
      if (msg.event != EventStartConnection &&
          msg.event != EventFinishConnection &&
          msg.event != EventConnectionStarted &&
          msg.event != EventConnectionFailed &&
          msg.event != EventConnectionFinished) {
        sz = readString<uint32_t>(data.substr(read_size), msg.session_id);
        if (!sz) {
          throw std::runtime_error("no enough SessionID bytes");
        }
        read_size += sz;
      }

      // connection id
      if (msg.event == EventConnectionStarted ||
          msg.event == EventConnectionFailed ||
          msg.event == EventConnectionFinished) {
        sz = readString<uint32_t>(data.substr(read_size), msg.connect_id);
        if (!sz) {
          throw std::runtime_error("no enough ConnectionID bytes");
        }
        read_size += sz;
      }
    }

    // payload
    sz = readString<uint32_t>(data.substr(read_size), msg.payload);
    if (!sz) {
      throw std::runtime_error("no enough Payload bytes");
    }
    read_size += sz;

    return msg;
  }

  std::string dump() {
    std::string data;

    // header
    char vers_n_size = 0x11;
    char type_n_flag = static_cast<char>(msg_type | msg_flag);
    char seri_n_comp = 0x10;
    char padding = 0x00;

    data += vers_n_size;
    data += type_n_flag;
    data += seri_n_comp;
    data += padding;

    // event
    if (msg_flag == MsgFlagWithEvent) {
      auto ev = static_cast<uint32_t>(event);
      writeBigEndian(data, ev);

      // session id
      if (event != EventStartConnection &&
          event != EventFinishConnection &&
          event != EventConnectionStarted &&
          event != EventConnectionFailed) {
        writeString<uint32_t>(data, session_id);
      }
    }

    // error code
    if (msg_type == MsgTypeError) {
      writeBigEndian(data, error_code);
    }

    // payload
    writeString<uint32_t>(data, payload);

    return data;
  }

  MsgType msg_type;
  MsgFlag msg_flag;
  Event event = EventNone;
  uint32_t error_code = 0;
  std::string session_id;
  std::string connect_id;
  std::string payload;
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
