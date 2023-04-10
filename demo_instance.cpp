#include <atomic>
#include <ratio>
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

class echohandler {
  private:
    lcm::LCM &inst;

  public:
    echohandler(lcm::LCM &inst) : inst(inst) {}
    ~echohandler() {}
    void handleMessage(const lcm::ReceiveBuffer *rbuf, const std::string &chan,
                       const exlcm::test_msg *msg) {
        inst.publish("channel2", msg);
    }
};

class ThroughputClient {
  private:
    lcm::LCM &inst;
    std::ofstream fs;
    const int msgsize = 1000; // use fixed 1KB message
    int recv_count{
        0}; // no need to use atomic - only read after thread finishes
    std::atomic_bool done{false};

    exlcm::test_msg my_data{0};

  public:
    ~ThroughputClient() { fs.close(); }

    ThroughputClient(lcm::LCM &inst, std::string filename) : inst(inst) {
        fs.open(filename);
        // create csv header
        fs << "mbps, percent_received\n";

        inst.subscribe("channel2", &ThroughputClient::handleMessage, this);
    }

    std::chrono::time_point<std::chrono::high_resolution_clock> sent;
    void handleMessage(const lcm::ReceiveBuffer *rbuf, const std::string &chan,
                       const exlcm::test_msg *msg) {
        if (msg->msgid >= 0)
            recv_count++; // otherwise we are in warmup stage
    }

    void run(double target_throughput_mbps) {
        my_data = exlcm::test_msg{0};
        my_data.data.resize(
            msgsize -
            2 * 64); // subtract size for dynamci array as well as msgid
        recv_count = 0;
        done = false;

        std::thread recvthread([this]() {
            while (!done) {
                inst.handleTimeout(1000); // so we finish sucessfully if no more
                                          // messages are queued up
            };
        });

        // discard a few messages for cache reasons
        auto discarded = 5;
        for (int i = 0; i < discarded; i++) {
            my_data.msgid = -1;
        }
        // and allow the server some time to catch its breath
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        my_data.msgid = 1; // valid msgid so server doesn't discard

        int num_messages = 100000;
        double msg_per_second = target_throughput_mbps * 1'000'000 / msgsize;
        double msg_interval_us = 1'000'000 / msg_per_second;
        std::chrono::microseconds msg_interval{
            static_cast<int>(msg_interval_us)};
        std::cout << "msg_interval_us: " << msg_interval.count() << "\n";

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_messages; i++) {
            auto next_msg_tp = start + i * msg_interval;
            std::this_thread::sleep_until(next_msg_tp);
            inst.publish("channel1", &my_data);
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
        done = true;
        recvthread.join();

        std::cout << "\t" << recv_count << "/" << num_messages;
        std::cout << " - got " << 100.0 * recv_count / num_messages << "percent"
                  << std::endl;
        fs << target_throughput_mbps << "," << 100.0 * recv_count / num_messages
           << "\n";
    }
};

class LatencyClient {
  private:
    lcm::LCM &inst;
    std::ofstream fs;
    const int num_messages = 1000;
    lcm::Subscription* subscription;

  public:
    ~LatencyClient() { fs.close(); 
        inst.unsubscribe(subscription);
    }

    LatencyClient(lcm::LCM &inst, std::string filename) : inst(inst) {
        subscription = inst.subscribe("channel2", &LatencyClient::handleMessage, this);

        fs.open(filename);
        // create csv header
        fs << "datasize, latency\n";
    }

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
                std::chrono::duration_cast<std::chrono::microseconds>(td)
                    .count());
            ;
        }
    }
    void run(int datasize, bool warmup = false) {
        std::cout << "client running with ds=" << datasize << ": ";

        results.clear();
        results.reserve(num_messages);
        my_data = exlcm::test_msg{0};

        my_data.size = datasize;
        my_data.data.resize(my_data.size);

        // discard first 10 runs for cache reasons
        auto discarded = 30;
        my_data.msgid = -discarded;

        for (int i = -discarded; i < num_messages; i++) {
            sent = std::chrono::high_resolution_clock::now();
            inst.publish("channel1", &my_data);
            inst.handle();
            my_data.msgid++;
        }

        if (!warmup) {
            for (int i : results) {
                fs << datasize << "," << i << "\n";
            }
        }
    }
};

enum class role { server, client };
static std::optional<role> r; // global for simplicity

static std::string instance_name;
std::vector<std::function<void(void)>> sendfunctions;
std::vector<std::chrono::high_resolution_clock::time_point> last_send;

void run_sim(std::string mcast_url,
             std::optional<std::vector<lcm_security_parameters>> sec_params) {

    std::unique_ptr<lcm::LCM> lcminst;
    bool security = !!sec_params;

    if (security)
        lcminst = std::make_unique<lcm::LCM>(mcast_url, sec_params->data(),
                                             sec_params->size());
    else
        lcminst = std::make_unique<lcm::LCM>(mcast_url);
    if (!lcminst->good())
        throw "lcmerror";

    assert(r);
    if (*r == role::server) {
        std::cout << "----SERVER-----" << std::endl;
        echohandler server(*lcminst);
        lcminst->subscribe("channel1", &echohandler::handleMessage, &server);
        if (security) {
            assert(lcminst);
            std::thread kxchg(&lcm::LCM::perform_keyexchange, *lcminst);
            while (0 == lcminst->handle()) {
            }
        } else {
            while (0 == lcminst->handle()) {
            }
        }
    } else {
        assert(*r == role::client);
        auto security_str = (security) ? "_secure" : "";
        std::cout << "----CLIENT-----" << std::endl;
        {
            std::cout << "----RUNNING ECHO TEST-----" << std::endl;
            LatencyClient c1(*lcminst, std::string("latency_results_full") +
                                           security_str + ".csv");
            std::cout << "Startup run-discard: ";
            c1.run(10, true);

            std::cout << std::endl;
            // fit log base 10 scale with 25 measurements between 100 and 100000
            for (int i :
                 {100,   133,   177,   237,   316,   421,   562,   749,   1000,
                  1333,  1778,  2371,  3162,  4217,  5623,  7499,  10000, 13335,
                  17783, 23713, 31623, 42170, 56234, 74989, 100000}) {
                c1.run(i);
            }
        }

        std::cout << "----RUNNING THROUGHPUT TEST-----" << std::endl;
        ThroughputClient c2(*lcminst, std::string("throughput_results") +
                                          security_str + ".csv");
        for (int i : {// 1, 2, 4, 8, 12,16,
                      20, 24, 28, 32}) {
            std::cout << "attempting bw of " << i << "MB/s - ";
            c2.run(i);
        }
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

        ////////////////////////////////////////////////////////////////////////
        // INFO: The parsing of the config file we do here makes little
        // sense/// for this application. it was mostly copy and pasted for
        // simplicity///
        ////////////////////////////////////////////////////////////////////////

        std::string algorithm =
            group1["algorithm"].value<std::string>().value();
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

        // setup subscriptions
        for (auto &entry : group1) {
            if (entry.second.is_table()) {
                // these are the channels
                auto &channel_config = *entry.second.as_table();

                auto send = channel_config["send"].value<bool>().value();
                auto recv = channel_config["receive"].value<bool>().value();
                if (recv) {
                    assert(!r);
                    r = role::server;
                } else if (send) {
                    assert(!r);
                    r = role::client;
                }
            }
        }

        bool security = true;
        if (security) {
            std::cout << "SECURITY" << std::endl;
            run_sim(multicast_url.value<std::string>().value(), sec_params);

        } else {
            std::cout << "NO SECURITY" << std::endl;
            run_sim(multicast_url.value<std::string>().value(), std::nullopt);
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
