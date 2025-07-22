//
//  speech_concept.hpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/7/22.
//

#pragma once

#include <any>
#include <functional>
#include <string>

namespace av {

namespace speech {

struct TTSRequest {
  std::string uuid;
  std::string text;
  std::any params;
};

struct SpeechEvent {
  std::string event; // open, close, error, sentence, audio, ...
  std::string uuid;
  std::any data;
};

using SpeechCallback = std::function<void(SpeechEvent&&)>;

} // speech

} // av
