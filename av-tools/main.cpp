//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>

#include "volc_tts.hpp"

using namespace av::speech;
using std::cout;
using std::cerr;
using std::endl;

int main(int argc, char* argv[]) {
  if (argc != 4) {
    cerr << "Usage: ./av-tools <appid> <token> <resid>\n";
    exit(EXIT_FAILURE);
  }

  const char* appid = argv[1];
  const char* token = argv[2];
  const char* resid = argv[3];

  try {
    boost::asio::io_context io;
    auto work_guard = boost::asio::make_work_guard(io);

    auto event_handler = [](VolcTTS::Event ev, std::string_view msg, std::string_view id) {
      std::ostringstream oss;

      oss << "event: " << ev;
      if (!id.empty()) {
        oss << ", id: " << id;
      }
      if (!msg.empty()) {
        oss << ", msg: " << msg;
      }
      oss << endl;

      cout << oss.str();
    };

    auto tts = VolcTTS::createVolcTTS(io, appid, token, resid, event_handler);

    auto t = std::thread([&tts, guard = std::move(work_guard)]() {
      tts->connect();
      std::this_thread::sleep_for(std::chrono::seconds(3));
      tts->teardown();
    });

    io.run();

    t.join();

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
