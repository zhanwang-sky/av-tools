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
#include "listener.hpp"

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

    auto event_handler = [&ofs](SpeechEvent&& ev) {
      std::ostringstream oss;
      auto p_data = std::any_cast<std::string>(&ev.data);

      oss << "event: " << ev.event;
      if (!ev.uuid.empty()) {
        oss << ", id: " << ev.uuid;
      }
      if (p_data) {
        if (ev.event == "audio") {
          ofs.write(p_data->data(), p_data->size());
        } else {
          oss << ", msg: " << *p_data;
        }
      }
      oss << endl;

      cout << oss.str();
    };

    boost::asio::io_context io;
    auto tts = VolcTTS::createVolcTTS(io, appid, token, resid, event_handler);

    auto t = std::thread([&tts, gurad = boost::asio::make_work_guard(io)]() {
      std::string session;
      std::string speaker;

      // session 1
      session = uuidgen(); // 生成sessionID
      speaker = "zh_female_meilinvyou_moon_bigtts"; // 指定音色

      tts->request({session, "哈哈，你", true}); // 传一个非法的speaker类型，会用默认值
      tts->request({session, "好呀，我是", {}}); // 后面就不用再传音色了

      tts->run(); // 可以先发起请求，再开始连接

      tts->request({session, "Sir", {}});
      tts->request({session, "i。", {}});
      tts->request({"", "", {}}); // sessionID为空，主动结束会话

      // session 2
      session = uuidgen(); // 生成新的sessionID
      speaker = "zh_male_beijingxiaoye_emo_v2_mars_bigtts"; // 指定音色

      tts->request({session, "听", speaker});
      tts->request({session, "说", {}});
      tts->request({session, "你", {}});
      tts->request({session, "", {}}); // 同一个sessionID但text为空，无效请求

      speaker = "zh_male_guangzhoudege_emo_mars_bigtts"; // 换一种音色
      tts->request({session, "很牛", speaker}); // 会话中改变音色无效
      tts->request({session, "逼", {}});
      tts->request({session, "啊", {}});

      // session 3
      session = uuidgen(); // uuid改变，开启新会话
      tts->request({session, "呵呵", speaker});
      tts->request({session, "哪", {}});
      tts->request({session, "里哪里", {}});
      tts->request({session, "。", {}});
      tts->request({session, "请问", {}});
      tts->request({session, "还", {}});
      tts->request({session, "有什", {}});
      tts->request({session, "么", {}});
      tts->request({session, "可以帮", {}});
      tts->request({session, "助您的？", {}});
      tts->request({"", "", {}}); // sessionID为空，主动结束会话

      std::this_thread::sleep_for(std::chrono::seconds(10));

      tts->close(); // 直接关闭WS连接
    });

    io.run();

    t.join();

  } catch (const std::exception& e) {
    cerr << "Exception caught: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
