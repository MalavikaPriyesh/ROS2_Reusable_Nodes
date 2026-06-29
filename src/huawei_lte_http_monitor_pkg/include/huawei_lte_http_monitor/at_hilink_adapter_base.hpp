#ifndef HUAWEI_LTE_HTTP_MONITOR__AT_HILINK_ADAPTER_BASE_HPP_
#define HUAWEI_LTE_HTTP_MONITOR__AT_HILINK_ADAPTER_BASE_HPP_

#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace huawei_lte_http_monitor
{

using namespace std::chrono_literals;

class AtHilinkAdapterBase : public rclcpp::Node
{
public:
  explicit AtHilinkAdapterBase(const std::string & node_name = "at_hilink_adapter_node")
  : Node(node_name)
  {
    state_pub_    = create_publisher<std_msgs::msg::String>("/network/at_hilink/state", 10);
    operator_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/operator", 10);
    rat_pub_      = create_publisher<std_msgs::msg::String>("/network/at_hilink/rat", 10);
    rssi_pub_     = create_publisher<std_msgs::msg::String>("/network/at_hilink/rssi_dbm", 10);
    rsrp_pub_     = create_publisher<std_msgs::msg::String>("/network/at_hilink/rsrp_dbm", 10);
    rsrq_pub_     = create_publisher<std_msgs::msg::String>("/network/at_hilink/rsrq_db", 10);
    sinr_pub_     = create_publisher<std_msgs::msg::String>("/network/at_hilink/sinr_db", 10);
    plmn_pub_     = create_publisher<std_msgs::msg::String>("/network/at_hilink/plmn", 10);
    summary_pub_  = create_publisher<std_msgs::msg::String>("/network/at_hilink/at_summary", 10);

    auto hb_qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
    hb_qos.deadline(2500ms);
    hb_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    hb_qos.liveliness_lease_duration(5000ms);
    heartbeat_pub_ = create_publisher<std_msgs::msg::String>("/network/at_hilink/heartbeat", hb_qos);

    timer_ = create_wall_timer(2000ms, std::bind(&AtHilinkAdapterBase::poll, this));
    RCLCPP_INFO(get_logger(), "AT-over-HiLink adapter base started");
  }

  virtual ~AtHilinkAdapterBase() = default;

protected:
  // Override these two to change the stick IP or commands
  virtual std::string fetch_signal() { return curl_get("/api/device/signal"); }
  virtual std::string fetch_plmn()   { return curl_get("/api/net/current-plmn"); }

  // Override if XML parsing must be customized
  virtual std::string extract_xml(const std::string & xml, const std::string & tag)
  {
    const std::string open = "<" + tag + ">";
    const std::string close = "</" + tag + ">";
    auto start = xml.find(open);
    if (start == std::string::npos) return "";
    start += open.size();
    auto end = xml.find(close, start);
    if (end == std::string::npos) return "";
    return xml.substr(start, end - start);
  }

  virtual std::string rat_to_text(const std::string & rat) const
  {
    if (rat == "7") return "LTE/4G";
    if (rat.empty()) return "";
    return "RAT " + rat;
  }

  virtual std::string value_or_dash(const std::string & val) const
  {
    return val.empty() ? "--" : val;
  }

  virtual void on_poll() {}  // hook after publishing

private:
  void poll()
  {
    const std::string signal = fetch_signal();
    const std::string plmn   = fetch_plmn();

    const std::string op = value_or_dash(extract_xml(plmn, "FullName"));
    const std::string rat = value_or_dash(rat_to_text(extract_xml(plmn, "Rat")));
    const std::string rssi = value_or_dash(extract_xml(signal, "rssi"));
    const std::string rsrp = value_or_dash(extract_xml(signal, "rsrp"));
    const std::string rsrq = value_or_dash(extract_xml(signal, "rsrq"));
    const std::string sinr = value_or_dash(extract_xml(signal, "sinr"));
    const std::string plmn_code = value_or_dash(extract_xml(plmn, "Numeric"));
    const bool reachable = !signal.empty() || !plmn.empty();

    publish_string(state_pub_, reachable ? "OK_FROM_AT" : "NO_RESPONSE_FROM_AT");
    publish_string(operator_pub_, "AT+COPS? -> " + op);
    publish_string(rat_pub_, "AT^SYSINFOEX -> " + rat);
    publish_string(rssi_pub_, "AT+CSQ -> " + rssi);
    publish_string(rsrp_pub_, "AT^HCSQ? -> " + rsrp);
    publish_string(rsrq_pub_, "AT^HCSQ? -> " + rsrq);
    publish_string(sinr_pub_, "AT^HCSQ? -> " + sinr);
    publish_string(plmn_pub_, "AT+COPS? -> " + plmn_code);
    publish_string(summary_pub_,
      "AT=OK; AT+CSQ=" + rssi + "; AT+COPS?=" + op +
      "; AT^SYSINFOEX=" + rat + "; AT^HCSQ?=" + rsrp + "," + rsrq + "," + sinr);
    publish_string(heartbeat_pub_, "at_hilink_adapter_node alive");

    if (!heartbeat_pub_->assert_liveliness())
      RCLCPP_WARN(get_logger(), "Failed to assert AT liveliness");

    on_poll();
  }

protected:
  std::string curl_get(const std::string & path)
  {
    const std::string base = "http://192.168.8.1";  // can be overridden by child
    const std::string cmd = "curl --connect-timeout 0.3 --max-time 0.8 -s " + base + path;
    std::array<char, 256> buf{};
    std::string result;
    FILE * pipe = popen(cmd.c_str(), "r");
    if (!pipe) return result;
    while (fgets(buf.data(), buf.size(), pipe) != nullptr) result += buf.data();
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
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr summary_pub_, heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

} // namespace huawei_lte_http_monitor
#endif
