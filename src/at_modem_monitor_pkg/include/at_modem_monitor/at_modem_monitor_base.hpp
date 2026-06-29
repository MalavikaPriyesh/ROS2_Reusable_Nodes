#ifndef AT_MODEM_MONITOR__AT_MODEM_MONITOR_BASE_HPP_
#define AT_MODEM_MONITOR__AT_MODEM_MONITOR_BASE_HPP_

#include <chrono>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace at_modem_monitor
{

using namespace std::chrono_literals;

class AtModemMonitorBase : public rclcpp::Node
{
public:
  explicit AtModemMonitorBase(const std::string & node_name = "at_modem_monitor_node")
  : Node(node_name)
  {
    // Parameters (can be overridden via command line)
    this->declare_parameter<bool>("mock_mode", true);
    this->declare_parameter<std::string>("serial_port", "/dev/ttyUSB0");
    mock_mode_    = this->get_parameter("mock_mode").as_bool();
    serial_port_  = this->get_parameter("serial_port").as_string();

    state_pub_    = create_publisher<std_msgs::msg::String>("/network/at_lte/state", 10);
    operator_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/operator", 10);
    rat_pub_      = create_publisher<std_msgs::msg::String>("/network/at_lte/rat", 10);
    rssi_pub_     = create_publisher<std_msgs::msg::String>("/network/at_lte/rssi_dbm", 10);
    rsrp_pub_     = create_publisher<std_msgs::msg::String>("/network/at_lte/rsrp_dbm", 10);
    rsrq_pub_     = create_publisher<std_msgs::msg::String>("/network/at_lte/rsrq_db", 10);
    sinr_pub_     = create_publisher<std_msgs::msg::String>("/network/at_lte/sinr_db", 10);
    plmn_pub_     = create_publisher<std_msgs::msg::String>("/network/at_lte/plmn", 10);

    auto hb_qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
    hb_qos.deadline(2500ms);
    hb_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    hb_qos.liveliness_lease_duration(5000ms);
    heartbeat_pub_ = create_publisher<std_msgs::msg::String>("/network/at_lte/heartbeat", hb_qos);

    timer_ = create_wall_timer(2000ms, std::bind(&AtModemMonitorBase::poll_modem, this));
    RCLCPP_INFO(get_logger(), "AT modem base started (mock=%s, port=%s)",
      mock_mode_ ? "true" : "false", serial_port_.c_str());
  }

  virtual ~AtModemMonitorBase() = default;

protected:
  // ---------- HOOKS – override in your plugin ----------

  /// Returns mock data as a comma-separated string:
  /// "state,operator,rat,rssi,rsrp,rsrq,sinr,plmn"
  virtual std::string generate_mock_data()
  {
    return "CONNECTED,MOCK_OPERATOR_AT+COPS,LTE/4G_AT^SYSINFOEX,-65dBm_AT+CSQ,-96dBm_AT^HCSQ,-12dB_AT^HCSQ,2dB_AT^HCSQ,26202_AT+COPS";
  }

  /// Reads real AT commands from the serial port.
  /// Override this with your actual serial communication code.
  virtual std::string read_real_at_data()
  {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 10000,
      "Real AT serial not implemented. Returning disconnected.");
    return "DISCONNECTED,--,--,--,--,--,--,--";
  }

  /// Called after every poll cycle (for custom reactions)
  virtual void on_poll() {}

private:
  void poll_modem()
  {
    std::string data;
    if (mock_mode_) {
      data = generate_mock_data();
    } else {
      data = read_real_at_data();
    }

    // Parse comma-separated values
    std::string parts[8];
    size_t start = 0, idx = 0;
    for (size_t i = 0; i < 8 && start < data.size(); ++i) {
      auto comma = data.find(',', start);
      if (comma == std::string::npos) comma = data.size();
      parts[i] = data.substr(start, comma - start);
      start = comma + 1;
    }

    publish_string(state_pub_,    parts[0].empty() ? "DISCONNECTED" : parts[0]);
    publish_string(operator_pub_, parts[1].empty() ? "--" : parts[1]);
    publish_string(rat_pub_,      parts[2].empty() ? "--" : parts[2]);
    publish_string(rssi_pub_,     parts[3].empty() ? "--" : parts[3]);
    publish_string(rsrp_pub_,     parts[4].empty() ? "--" : parts[4]);
    publish_string(rsrq_pub_,     parts[5].empty() ? "--" : parts[5]);
    publish_string(sinr_pub_,     parts[6].empty() ? "--" : parts[6]);
    publish_string(plmn_pub_,     parts[7].empty() ? "--" : parts[7]);
    publish_string(heartbeat_pub_, "at_modem_monitor_node alive");

    if (!heartbeat_pub_->assert_liveliness())
      RCLCPP_WARN(get_logger(), "Failed to assert AT modem liveliness");

    on_poll();
  }

  void publish_string(const auto & pub, const std::string & value)
  {
    std_msgs::msg::String msg; msg.data = value; pub->publish(msg);
  }

  bool mock_mode_{true};
  std::string serial_port_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub_, operator_pub_, rat_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr rssi_pub_, rsrp_pub_, rsrq_pub_, sinr_pub_, plmn_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

} // namespace at_modem_monitor
#endif
