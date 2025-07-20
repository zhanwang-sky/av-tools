//
//  main.cpp
//  av-tools
//
//  Created by zhanwang-sky on 2025/2/7.
//

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "volc_tts.hpp"

using namespace av::speech;
using std::cout;
using std::cerr;
using std::endl;

std::string uuidgen() {
  boost::uuids::random_generator generator;
  boost::uuids::uuid uuid = generator();
  return to_string(uuid);
}

int main(int argc, char* argv[]) {
  if (argc != 5) {
    cerr << "Usage: ./av-tools <appid> <token> <resid> <ofile>\n";
    exit(EXIT_FAILURE);
  }

  const char* appid = argv[1];
  const char* token = argv[2];
  const char* resid = argv[3];
  const char* ofile = argv[4];

  try {
    std::ofstream ofs(ofile, std::fstream::trunc | std::fstream::binary);
    if (!ofs) {
      throw std::runtime_error("fail to create output file");
    }

    auto event_handler = [&ofs](VolcTTS::Event ev, std::string_view id, std::string_view msg) {
      std::ostringstream oss;

      oss << "event: " << ev;
      if (!id.empty()) {
        oss << ", id: " << id;
      }
      if (!msg.empty()) {
        if (ev == VolcTTS::EventTTSAudio) {
          ofs.write(msg.data(), msg.size());
        } else {
          oss << ", msg: " << msg;
        }
      }
      oss << endl;

      cout << oss.str();
    };

    boost::asio::io_context io;
    auto tts = VolcTTS::createVolcTTS(io, appid, token, resid, event_handler);

    auto t = std::thread([&tts, gurad = boost::asio::make_work_guard(io)]() {
      std::string session;

      // session 1
      session = uuidgen(); // 生成sessionID
      tts->request({session, "zh_female_meilinvyou_moon_bigtts", "我"});
      tts->request({session, "zh_female_meilinvyou_moon_bigtts", "是小"});
      tts->connect(); // 可以先发起请求，再开始连接
      tts->request({session, "", "明"});
      tts->request({session, "", "。"});
      tts->request({"", "", "。"}); // sessionID为空，主动结束会话

      // session 2
      session = uuidgen(); // 生成新的sessionID
      tts->request({session, "zh_male_beijingxiaoye_emo_v2_mars_bigtts", "听"});
      tts->request({session, "", "说"});
      tts->request({session, "", "你"});
      tts->request({session, "", ""}); // sessionID不变、text为空，无效请求
      tts->request({session, "zh_male_guangzhoudege_emo_mars_bigtts", "很牛"}); // 会话中改变音色无效
      tts->request({session, "", "逼"});
      tts->request({session, "", "啊。"});

      // session 3
      session = uuidgen(); // uuid改变，开启新会话
      tts->request({session, "zh_male_yourougongzi_emo_v2_mars_bigtts", "我"});
      tts->request({session, "", "们改"});
      tts->request({session, "", "日再"});
      tts->request({session, "", "聊"});
      tts->request({session, "zh_male_guangzhoudege_emo_mars_bigtts", ""}); // 换音色无效
      tts->request({session, "", "，再见"});
      tts->request({"", "", "再见"}); // sessionID为空，主动结束会话

      std::this_thread::sleep_for(std::chrono::seconds(10));

      tts->teardown(); // 直接关闭WS连接
    });

    io.run();

    t.join();

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
