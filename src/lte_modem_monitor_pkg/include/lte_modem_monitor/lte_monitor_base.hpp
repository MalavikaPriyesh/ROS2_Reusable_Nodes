#ifndef LTE_MODEM_MONITOR__LTE_MONITOR_BASE_HPP_
#define LTE_MODEM_MONITOR__LTE_MONITOR_BASE_HPP_

#include <array>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace lte_modem_monitor
{

using namespace std::chrono_literals;

class LteMonitorBase : public rclcpp::Node
{
public:
  explicit LteMonitorBase(const std::string & node_name = "lte_monitor_node")
  : Node(node_name)
  {
    state_pub_    = create_publisher<std_msgs::msg::String>("/network/lte/state", 10);
    operator_pub_ = create_publisher<std_msgs::msg::String>("/network/lte/operator", 10);
    rat_pub_      = create_publisher<std_msgs::msg::String>("/network/lte/rat", 10);
    rssi_pub_     = create_publisher<std_msgs::msg::String>("/network/lte/rssi_dbm", 10);
    rsrp_pub_     = create_publisher<std_msgs::msg::String>("/network/lte/rsrp_dbm", 10);
    rsrq_pub_     = create_publisher<std_msgs::msg::String>("/network/lte/rsrq_db", 10);
    sinr_pub_     = create_publisher<std_msgs::msg::String>("/network/lte/sinr_db", 10);
    plmn_pub_     = create_publisher<std_msgs::msg::String>("/network/lte/plmn", 10);

    auto hb_qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
    hb_qos.deadline(2500ms);
    hb_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    hb_qos.liveliness_lease_duration(5000ms);
    heartbeat_pub_ = create_publisher<std_msgs::msg::String>("/network/lte/heartbeat", hb_qos);

    timer_ = create_wall_timer(1000ms, std::bind(&LteMonitorBase::poll_lte, this));
    RCLCPP_INFO(get_logger(), "LTE monitor base started");
  }

  virtual ~LteMonitorBase() = default;

protected:
  // ---------- HOOKS – override in your plugin ----------

  /// Returns the status rows from `nmcli device status`
  virtual std::string fetch_device_status()
  {
    return run_command("nmcli -t -f DEVICE,TYPE,STATE,CONNECTION device status");
  }

  /// Returns the interface name to search for (e.g., "enx001e101f0000")
  virtual std::string get_lte_interface() const { return "enx001e101f0000"; }

  /// Returns raw data from the Huawei stick (signal XML)
  virtual std::string fetch_huawei_signal()
  {
    return run_command("curl --connect-timeout 0.3 --max-time 0.8 -s http://192.168.8.1/api/device/signal");
  }

  /// Returns raw data from the Huawei stick (PLMN XML)
  virtual std::string fetch_huawei_plmn()
  {
    return run_command("curl --connect-timeout 0.3 --max-time 0.8 -s http://192.168.8.1/api/net/current-plmn");
  }

  /// Extracts value from XML by tag name
  virtual std::string xml_value(const std::string & xml, const std::string & tag) const
  {
    const std::string open = "<" + tag + ">";
    const std::string close = "</" + tag + ">";
    const auto start = xml.find(open);
    if (start == std::string::npos) return "";
    const auto value_start = start + open.size();
    const auto end = xml.find(close, value_start);
    if (end == std::string::npos) return "";
    return xml.substr(value_start, end - value_start);
  }

  /// Converts numeric RAT to string
  virtual std::string rat_text(const std::string & rat) const
  {
    if (rat == "7") return "LTE/4G";
    if (rat.empty()) return "";
    return "RAT " + rat;
  }

  /// Returns "--" for empty values
  virtual std::string dash(const std::string & value) const
  {
    return value.empty() ? "--" : value;
  }

  /// Called after every poll cycle (for custom reactions)
  virtual void on_poll() {}

private:
  struct Device { std::string name; std::string state; std::string connection; };

  void poll_lte()
  {
    const Device lte = find_lte_device();
    const bool connected = lte.state == "connected" || lte.state.rfind("connected ", 0) == 0;

    const std::string signal = fetch_huawei_signal();
    const std::string plmn   = fetch_huawei_plmn();

    const std::string op   = dash(xml_value(plmn, "FullName"));
    const std::string rat  = dash(rat_text(xml_value(plmn, "Rat")));
    const std::string rssi = dash(xml_value(signal, "rssi"));
    const std::string rsrp = dash(xml_value(signal, "rsrp"));
    const std::string rsrq = dash(xml_value(signal, "rsrq"));
    const std::string sinr = dash(xml_value(signal, "sinr"));
    const std::string plmn_code = dash(xml_value(plmn, "Numeric"));

    publish_string(state_pub_, connected ? "CONNECTED" : "DISCONNECTED");
    publish_string(operator_pub_, op);
    publish_string(rat_pub_, rat);
    publish_string(rssi_pub_, rssi);
    publish_string(rsrp_pub_, rsrp);
    publish_string(rsrq_pub_, rsrq);
    publish_string(sinr_pub_, sinr);
    publish_string(plmn_pub_, plmn_code);
    publish_string(heartbeat_pub_, "lte_monitor_node alive");

    if (!heartbeat_pub_->assert_liveliness())
      RCLCPP_WARN(get_logger(), "Failed to assert LTE liveliness");

    on_poll();
  }

  Device find_lte_device()
  {
    const std::string output = fetch_device_status();
    const std::string target = get_lte_interface();
    std::stringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
      std::stringstream ls(line);
      std::string name, type, state, connection;
      std::getline(ls, name, ':');
      std::getline(ls, type, ':');
      std::getline(ls, state, ':');
      std::getline(ls, connection);
      if (name == target) return {name, state, connection};
    }
    return {target, "missing", "--"};
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

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub_, operator_pub_, rat_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr rssi_pub_, rsrp_pub_, rsrq_pub_, sinr_pub_, plmn_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

} // namespace lte_modem_monitor
#endif
