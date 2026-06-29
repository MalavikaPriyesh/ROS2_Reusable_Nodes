#ifndef NETWORK_FUSION__NETWORK_FUSION_BASE_HPP_
#define NETWORK_FUSION__NETWORK_FUSION_BASE_HPP_

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace network_fusion
{

using namespace std::chrono_literals;

/// @brief Reusable network fusion engine.
///        The plugin registers any number of links via add_link().
///        The engine performs priority-based failover automatically.
class NetworkFusionBase : public rclcpp::Node
{
public:
  explicit NetworkFusionBase(const std::string & node_name = "network_fusion_node")
  : Node(node_name)
  {
    status_pub_ = create_publisher<std_msgs::msg::String>("/network_status", 10);
    reason_pub_ = create_publisher<std_msgs::msg::String>("/network_reason", 10);

    auto hb_qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
    hb_qos.deadline(5000ms);
    hb_qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC);
    hb_qos.liveliness_lease_duration(8000ms);
    heartbeat_pub_ = create_publisher<std_msgs::msg::String>("/network/heartbeat", hb_qos);

    timer_ = create_wall_timer(400ms, std::bind(&NetworkFusionBase::publish_status, this));
    RCLCPP_INFO(get_logger(), "Network fusion base started");
  }

  virtual ~NetworkFusionBase() = default;

protected:
  struct Link
  {
    std::string name;        // e.g. "WIFI"
    std::string state{"DISCONNECTED"};
    rclcpp::Time seen{0, 0, RCL_ROS_TIME};
    int priority{0};         // lower number = higher preference
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub;
  };

  /// ⭐ THE KEY TO REUSABILITY ⭐
  /// Plugin calls this to register ANY link with ANY topic name and priority.
  /// priority 0 = most preferred (primary), higher numbers = fallback order.
  void add_link(const std::string & name, const std::string & topic, int priority)
  {
    auto link = std::make_shared<Link>();
    link->name = name;
    link->priority = priority;
    link->sub = create_subscription<std_msgs::msg::String>(
      topic, 10,
      [this, name](std_msgs::msg::String::SharedPtr msg) {
        auto it = links_.find(name);
        if (it != links_.end()) {
          it->second->state = msg->data;
          it->second->seen = now();
        }
      });
    links_[name] = link;
    RCLCPP_INFO(get_logger(),
      "Registered link '%s' on topic '%s' (priority %d)",
      name.c_str(), topic.c_str(), priority);
  }

  // ---------- HOOKS – override in your plugin ----------

  /// Max age (seconds) before a link's data is considered STALE.
  virtual double freshness_timeout_s() const { return 10.0; }

  /// Called after every publish (custom reactions, e.g. failsafe).
  virtual void on_status(const std::string & /*status*/, const std::string & /*active*/) {}

private:
  void publish_status()
  {
    const double timeout = freshness_timeout_s();
    std::string active = "NONE";
    int best_priority = std::numeric_limits<int>::max();
    std::string reason;

    for (auto & [name, link] : links_) {
      const bool fresh = link->seen.nanoseconds() != 0 &&
                         (now() - link->seen).seconds() <= timeout;
      const bool connected = fresh && link->state == "CONNECTED";

      reason += name + "=" + (fresh ? link->state : "STALE") + ",";

      if (connected && link->priority < best_priority) {
        best_priority = link->priority;
        active = name;
      }
    }

    std::string status;
    if (active == "NONE") {
      status = "NETWORK_UNHEALTHY";
    } else if (best_priority == top_priority()) {
      status = "NETWORK_HEALTHY";   // primary link is up
    } else {
      status = "NETWORK_BACKUP";    // running on a fallback link
    }

    reason += "ACTIVE_CONNECTION=" + active;

    publish_string(status_pub_, status);
    publish_string(reason_pub_, reason);
    publish_string(heartbeat_pub_, "network_fusion_node alive");

    if (!heartbeat_pub_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert network liveliness");
    }

    on_status(status, active);
  }

  int top_priority() const
  {
    int best = std::numeric_limits<int>::max();
    for (const auto & [name, link] : links_) {
      best = std::min(best, link->priority);
    }
    return best;
  }

  void publish_string(
    const rclcpp::Publisher<std_msgs::msg::String>::SharedPtr & pub,
    const std::string & value)
  {
    std_msgs::msg::String msg; msg.data = value; pub->publish(msg);
  }

  std::map<std::string, std::shared_ptr<Link>> links_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_, reason_pub_, heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

} // namespace network_fusion
#endif
