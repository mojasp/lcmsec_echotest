// file: listener.cpp
//
// LCM example program.
//
// compile with:
//  $ gcc -o listener listener.cpp -llcm
//
// On a system with pkg-config, you can also use:
//  $ gcc -o listener listener.cpp `pkg-config --cflags --libs lcm`

#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <lcm/lcm-cpp.hpp>
#include <list>
#include <memory>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <thread>

#include "exlcm/test_msg.hpp"
#include "tomlplusplus/toml.hpp"
using namespace std::chrono_literals;

constexpr std::chrono::duration sender_delay(2s);

std::unique_ptr<lcm::LCM> lcminst;
bool security = true;

class echohandler {
private:
public:
  ~echohandler() {}
  echohandler() {}
  void handleMessage(const lcm::ReceiveBuffer *rbuf, const std::string &chan,
                     const exlcm::test_msg *msg) {
    lcminst->publish("channel2", msg);
  }
};

class LatencyClient {
private:
public:
  ~LatencyClient() {}
  LatencyClient() {}

  const int num_messages = 200;
  exlcm::test_msg my_data{0};

  std::vector<int> results;
  std::chrono::time_point<std::chrono::high_resolution_clock> sent;
  void handleMessage(const lcm::ReceiveBuffer *rbuf, const std::string &chan,
                     const exlcm::test_msg *msg) {
    auto now = std::chrono::high_resolution_clock::now();
    auto td = now - sent;
    assert(msg->msgid == my_data.msgid);
    if (msg->msgid >= 0) {
      results.push_back(
          std::chrono::duration_cast<std::chrono::microseconds>(td).count());
      ;
    }
  }
  void run() {
    my_data.size = 100000;
    my_data.data.resize(my_data.size);

    // discard first 10 runs for cache reasons
    auto discarded = 10;
    my_data.msgid = -discarded;

    lcminst->subscribe("channel2", &LatencyClient::handleMessage, this);

    for (int i = -discarded; i < num_messages; i++) {
      sent = std::chrono::high_resolution_clock::now();
      lcminst->publish("channel1", &my_data);
      lcminst->handle();
      my_data.msgid++;
    }
    // remove max and min elements
    assert(results.size() == num_messages);
    for (int i = 0; i < 3; i++) {
      auto max = std::max_element(results.begin(), results.end());
      auto min = std::min_element(results.begin(), results.end());
      results.erase(max);
      results.erase(min);
    }

    // avg:
    auto avg = static_cast<double>(
                   std::accumulate(results.begin(), results.end(), 0)) /
               results.size();
    std::cout << "got average " << avg << " us" << std::endl;
  }
};

enum class role { server, client };
static std::optional<role> r; // global for simplicity

echohandler handlerObject{};
LatencyClient client_handler{};
static std::string instance_name;
std::vector<std::function<void(void)>> sendfunctions;
std::vector<std::chrono::high_resolution_clock::time_point> last_send;

void setup_channel(toml::table &channel_config) {
  auto channelname = channel_config["channelname"].value<std::string>().value();
  auto send = channel_config["send"].value<bool>().value();
  auto recv = channel_config["receive"].value<bool>().value();
  if (recv) {
    assert(!r);
    assert(channelname == "channel1");
    std::cout << instance_name << ": subscribing to channel " << channelname
              << "\n";
    lcminst->subscribe(channelname, &echohandler::handleMessage,
                       &handlerObject);
    r = role::server;
  } else if (send) {
    assert(!r);
    assert(channelname == "channel1");
    r = role::client;
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: ./demo_instance <path_to_config_file>\n";
    return 1;
  }
  toml::table config;
  try {
    config = toml::parse_file(argv[1]);
    std::cout << config << "\n";
  } catch (const toml::parse_error &err) {
    std::cerr << "Parsing config file failed failed:\n" << err << "\n";
    return 1;
  }

  try {
    auto groups = config["group"].as_table();
    if (!groups) {
      std::cout << "multicast group URL must be specified \n";
      return 1;
    }
    if (groups->size() != 1) {
      std::cout << "multiple groups not supported atm\n";
      return 1;
    }
    instance_name = config["instance_name"].value<std::string>().value();

    // get and init lcm multicast url
    auto group1key = groups->begin()->first;
    auto &group1 = *(*groups)[group1key].as_table();

    auto multicast_url = group1["multicast_url"];

    // init security parameters
    std::vector<lcm_security_parameters> sec_params;

    std::string algorithm = group1["algorithm"].value<std::string>().value();
    std::string privkey = group1["privkey"].value<std::string>().value();
    std::string cert = group1["cert"].value<std::string>().value();
    std::string root_ca = group1["root_ca"].value<std::string>().value();

    lcm_security_parameters group_params;
    group_params.algorithm = algorithm.data();
    group_params.certificate = cert.data();
    group_params.keyfile = privkey.data();
    group_params.root_ca = root_ca.data();
    group_params.keyexchange_in_background = false;
    sec_params.push_back(group_params);

    if (security)
      lcminst =
          std::make_unique<lcm::LCM>(multicast_url.value<std::string>().value(),
                                     sec_params.data(), sec_params.size());
    else
      lcminst = std::make_unique<lcm::LCM>(
          multicast_url.value<std::string>().value());
    if (!lcminst->good())
      return 1;

    // setup subscriptions
    for (auto &entry : group1) {
      if (entry.second.is_table()) {
        // these are the channels
        auto &channel_config = *entry.second.as_table();

        setup_channel(channel_config);
      }
    }

    assert(r);
    if (*r == role::server) {
      std::cout << "----SERVER-----" << std::endl;
      if (security)
        std::thread kxchg(&lcm::LCM::perform_keyexchange, *lcminst);
      while (0 == lcminst->handle()) {
      }
    } else {
      assert(*r == role::client);
      std::cout << "----CLIENT-----" << std::endl;
      LatencyClient c;
      c.run();
    }
  } catch (const std::bad_optional_access &err) {
    std::cerr << "missing configuration: " << err.what() << "\n";

    // threads might have been initialized already, try to shutdown
    // gracefully...
    std::cerr << "joining already dispatched threads... \n";
    return 1;
  }
  return 0;
}
