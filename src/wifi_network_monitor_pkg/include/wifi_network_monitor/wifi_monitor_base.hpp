#ifndef WIFI_NETWORK_MONITOR__WIFI_MONITOR_BASE_HPP_
#define WIFI_NETWORK_MONITOR__WIFI_MONITOR_BASE_HPP_

#include <array>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"

namespace wifi_network_monitor
{

using namespace std::chrono_literals;

class WifiMonitorBase : public rclcpp::Node
{
public:
  explicit WifiMonitorBase(const std::string & node_name = "wifi_monitor_node")
  : Node(node_name)
  {
    state_pub_    = create_publisher<std_msgs::msg::String>("/network/wifi/state", 10);
    ssid_pub_     = create_publisher<std_msgs::msg::String>("/network/wifi/connected_ssid", 10);
    speed_pub_    = create_publisher<std_msgs::msg::Int32>("/network/wifi/link_speed_mbps", 10);
    ssids_pub_    = create_publisher<std_msgs::msg::String>("/network/wifi/available_ssids", 10);
    
    auto hb_qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
    hb_qos.deadline(5000ms);
    hb_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    hb_qos.liveliness_lease_duration(8000ms);
    heartbeat_pub_ = create_publisher<std_msgs::msg::String>("/network/wifi/heartbeat", hb_qos);
    
    timer_ = create_wall_timer(1000ms, std::bind(&WifiMonitorBase::publish_wifi, this));
    RCLCPP_INFO(get_logger(), "WiFi monitor base started");
  }

  virtual ~WifiMonitorBase() = default;

protected:
  virtual std::string fetch_raw_data()
  {
    return run_command("nmcli -t -f ACTIVE,SSID,RATE,SECURITY dev wifi");
  }
  
  virtual void parse_data(
    const std::string & raw,
    std::string & state,
    std::string & connected_ssid,
    std::string & available_ssids,
    int & speed_mbps)
  {
    std::stringstream stream(raw);
    std::string line;
    bool first_ssid = true;
    state = "DISCONNECTED";
    connected_ssid = "--";
    available_ssids.clear();
    speed_mbps = -1;

    while (std::getline(stream, line)) {
      if (line.empty()) continue;
      std::stringstream ls(line);
      std::string active, ssid, rate, security;
      std::getline(ls, active, ':');
      std::getline(ls, ssid, ':');
      std::getline(ls, rate, ':');
      std::getline(ls, security);
      if (!ssid.empty()) {
        if (!first_ssid) available_ssids += ", ";
        available_ssids += ssid;
        first_ssid = false;
      }
      if (active == "yes") {
        state = "CONNECTED";
        connected_ssid = ssid.empty() ? "--" : ssid;
        try { speed_mbps = std::stoi(rate); } catch (...) { speed_mbps = -1; }
      }
    }
  }

  virtual void on_publish(const std::string & /*state*/) {}

private:
  void publish_wifi()
  {
    const auto raw = fetch_raw_data();
    std::string state, connected_ssid, available_ssids;
    int speed_mbps;
    parse_data(raw, state, connected_ssid, available_ssids, speed_mbps);

    publish_string(state_pub_, state);
    publish_string(ssid_pub_, connected_ssid);
    publish_int(speed_pub_, speed_mbps);
    publish_string(ssids_pub_, available_ssids);
    publish_string(heartbeat_pub_, "wifi_monitor_node alive");

    if (!heartbeat_pub_->assert_liveliness())
      RCLCPP_WARN(get_logger(), "Failed to assert network liveliness");

    on_publish(state);
  }

protected:
  std::string run_command(const std::string & command)
  {
    std::array<char, 256> buffer{};
    std::string result;
    FILE * pipe = popen(command.c_str(), "r");
    if (!pipe) return result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
      result += buffer.data();
    pclose(pipe);
    return result;
  }

private:
  void publish_string(const auto & pub, const std::string & value)
  {
    std_msgs::msg::String msg; msg.data = value; pub->publish(msg);
  }
  void publish_int(const auto & pub, int value)
  {
    std_msgs::msg::Int32 msg; msg.data = value; pub->publish(msg);
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub_, ssid_pub_, ssids_pub_, heartbeat_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr speed_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

} // namespace wifi_network_monitor
#endif
