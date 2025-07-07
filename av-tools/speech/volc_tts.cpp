//
//  volc_tts.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/2.
//

#include <nlohmann/json.hpp>
#include "volc_tts.hpp"

using namespace nlohmann;
using namespace av::speech;

template <typename T>
inline std::size_t writeBigEndian(std::string& data, T val) {
  for (std::size_t i = sizeof(T); i-- != 0; ) {
    data += static_cast<char>((val >> (8 * i)) & 0xff);
  }
  return sizeof(T);
}

template <typename T>
inline std::size_t readBigEndian(const std::string& data, T& val) {
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
inline std::size_t writeString(std::string& data, const std::string& val) {
  auto len = static_cast<T>(val.length());
  writeBigEndian(data, len);
  data += val;
  return sizeof(T) + len;
}

template <typename T>
inline std::size_t readString(const std::string& data, std::string& val) {
  T len = 0;
  if (!readBigEndian(data, len) || (data.length() < sizeof(T) + len)) {
    return 0;
  }
  val = std::string(data.data() + sizeof(T), len);
  return sizeof(T) + len;
}

VolcTTS::Message VolcTTS::Message::parse(const std::string& data) {
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

std::string VolcTTS::Message::dump() {
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

VolcTTS::VolcTTS(boost::asio::io_context& io, callback_type&& cb,
                 const std::string& app_key, const std::string& access_key,
                 const std::string& resource_id, const std::string& connect_id,
                 const std::string& session_id, const std::string& speaker)
    : utils::Websocket(io, host, "443", url,
                       {
                         {"X-Api-App-Key", app_key},
                         {"X-Api-Access-Key", access_key},
                         {"X-Api-Resource-Id", resource_id},
                         {"X-Api-Connect-Id", connect_id},
                       }),
      cb_(std::move(cb)),
      session_id_(session_id),
      speaker_(speaker)
{
}

VolcTTS::~VolcTTS() { }

void VolcTTS::start() {
  run();
}

void VolcTTS::request(const std::string& text) {
  auto p_text = std::make_shared<std::string>(text);
  boost::asio::post(strand_,
                    boost::beast::bind_front_handler(&VolcTTS::on_post_request,
                                                     shared_from_base<VolcTTS>(),
                                                     p_text));
}

void VolcTTS::stop(bool force) {
  if (force) {
    close();
    return;
  }

  boost::asio::post(strand_,
                    boost::beast::bind_front_handler(&VolcTTS::on_post_stop,
                                                     shared_from_base<VolcTTS>(),
                                                     force));
}

void VolcTTS::on_open() {
  state_ = 1;
  cb_(EventConnOpen, "");
  start_connection();
}

void VolcTTS::on_close() {
  state_ = 7;
  cb_(EventConnClose, "");
}

void VolcTTS::on_message(const std::string& msg) {
  try {
    Message volc_msg = Message::parse(msg);

    if (volc_msg.msg_type == Message::MsgTypeFullServer &&
        volc_msg.msg_flag == Message::MsgFlagWithEvent) {
      switch (volc_msg.event) {
        case Message::EventConnectionStarted:
          if (state_ == 1) {
            state_ = 2;
            start_tts_session();
          }
          break;

        case Message::EventSessionStarted:
          if (state_ == 2) {
            state_ = 3;
            cb_(EventTTSStart, volc_msg.payload);
          }
          break;

        case Message::EventTTSSentenceStart:
        case Message::EventTTSSentenceEnd:
          if (state_ == 3) {
            cb_(EventTTSMessage, volc_msg.payload);
          }
          break;

        case Message::EventSessionFinished:
          if (state_ < 4) {
            state_ = 4;
            finish_connection();
            cb_(EventTTSStop, volc_msg.payload);
          }
          break;

        case Message::EventConnectionFinished:
          if (state_ < 5) {
            state_ = 5;
            on_post_stop(true);
          }
          break;

        case Message::EventConnectionFailed:
        case Message::EventSessionFailed:
          if (state_ < 7) {
            state_ = 6;
            on_post_stop(true);
            cb_(EventTTSError, volc_msg.payload);
          }
          break;

        default:
          break;
      }
    }

    if (volc_msg.msg_type == Message::MsgTypeAudioOnlyServer &&
        volc_msg.msg_flag == Message::MsgFlagWithEvent) {
      if (volc_msg.event == Message::EventTTSResponse) {
        if (state_ == 3) {
          cb_(EventTTSAudio, volc_msg.payload);
        }
      }
    }

  } catch (const std::exception& e) {
    if (state_ < 7) {
      state_ = 6;
      on_post_stop(true);
      cb_(EventTTSError, "malformed tts message");
    }
  }
}

void VolcTTS::on_error(const std::exception& e) {
  cb_(EventConnError, e.what());
}

void VolcTTS::on_post_request(std::shared_ptr<std::string> p_text) {
  if (state_ == 3) {
    send_tts_message(p_text);
  }
}

void VolcTTS::on_post_stop(bool force) {
  if (!force && state_ == 3) {
    finish_tts_session();
  } else {
    close();
  }
}

void VolcTTS::start_connection() {
  Message msg;
  msg.msg_type = Message::MsgTypeFullClient;
  msg.msg_flag = Message::MsgFlagWithEvent;
  msg.event = Message::EventStartConnection;
  msg.payload = "{}";

  send(msg.dump());
}

void VolcTTS::start_tts_session() {
  Message msg;
  msg.msg_type = Message::MsgTypeFullClient;
  msg.msg_flag = Message::MsgFlagWithEvent;
  msg.event = Message::EventStartSession;
  msg.session_id = session_id_;
  msg.payload = make_json_payload(msg.event);

  send(msg.dump());
}

void VolcTTS::send_tts_message(std::shared_ptr<std::string> p_text) {
  Message msg;
  msg.msg_type = Message::MsgTypeFullClient;
  msg.msg_flag = Message::MsgFlagWithEvent;
  msg.event = Message::EventTaskRequest;
  msg.session_id = session_id_;
  msg.payload = make_json_payload(msg.event, p_text);

  send(msg.dump());
}

void VolcTTS::finish_tts_session() {
  Message msg;
  msg.msg_type = Message::MsgTypeFullClient;
  msg.msg_flag = Message::MsgFlagWithEvent;
  msg.event = Message::EventFinishSession;
  msg.session_id = session_id_;
  msg.payload = "{}";

  send(msg.dump());
}

void VolcTTS::finish_connection() {
  Message msg;
  msg.msg_type = Message::MsgTypeFullClient;
  msg.msg_flag = Message::MsgFlagWithEvent;
  msg.event = Message::EventFinishConnection;
  msg.payload = "{}";

  send(msg.dump());
}

std::string VolcTTS::make_json_payload(Message::Event event,
                                       std::shared_ptr<std::string> p_text) {
  json json_payload;
  json_payload["event"] = event;
  json_payload["namespace"] = ns;

  auto& req_params = json_payload["req_params"];
  req_params["speaker"] = speaker_;
  if (p_text) {
    req_params["text"] = *p_text;
  }

  auto& audio_params = req_params["audio_params"];
  audio_params["format"] = "pcm";
  audio_params["sample_rate"] = 16000;

  return json_payload.dump();
}
