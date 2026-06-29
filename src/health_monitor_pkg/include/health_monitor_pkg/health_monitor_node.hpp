#ifndef HEALTH_MONITOR_PKG__HEALTH_MONITOR_NODE_HPP_
#define HEALTH_MONITOR_PKG__HEALTH_MONITOR_NODE_HPP_

#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "rclcpp/rclcpp.hpp"

// Default project interfaces (can be overridden via template)
#include "drone_health_interfaces/msg/health_status.hpp"
#include "drone_health_interfaces/msg/management_state.hpp"

namespace health_monitor_pkg
{

template<
  typename HealthStatusMsg = drone_health_interfaces::msg::HealthStatus,
  typename ManagementStateMsg = drone_health_interfaces::msg::ManagementState>
class HealthMonitorNode : public rclcpp::Node
{
public:
  using HealthStatus = HealthStatusMsg;
  using ManagementState = ManagementStateMsg;

  HealthMonitorNode()
  : Node("health_monitor_node")
  {
    declare_base_parameters();
    read_base_parameters();
    declare_monitor_parameters();
    read_monitor_parameters();
    setup_publisher();
    setup_management_subscription();
    register_user_message_types();
    setup_subscriptions();
    setup_timer();

    RCLCPP_INFO(get_logger(), "Health monitor node started");
  }

  // FACTORY METHOD: Add new sensor types without editing this class
  template<typename MsgT>
  void register_message_type(const std::string & type_name)
  {
    subscription_factories_[type_name] =
      [this](const std::string & id, const std::string & topic_name,
        const rclcpp::QoS & qos, const rclcpp::SubscriptionOptions & options)
      {
        auto sub = this->template create_subscription<MsgT>(
          topic_name, qos,
          [this, id](const std::shared_ptr<MsgT>) {handle_message(id);},
          options);
        generic_subscriptions_.push_back(sub);
      };
  }

protected:
  // Override this in derived classes to register sensors
  virtual void register_user_message_types() {}

private:
  struct MonitorConfig
  {
    std::string id;
    std::string node_name;
    std::string topic_name;
    std::string kind;
    std::string message_type;
    std::string reliability;
    int deadline_ms;
    int liveliness_ms;
    int timeout_ms;
    bool has_liveliness;
    bool seen{false};
    rclcpp::Time last_update{0, 0, RCL_ROS_TIME};
    rclcpp::Time last_status_publish{0, 0, RCL_ROS_TIME};
  };

  using SubscriptionFactory = std::function<void(
        const std::string & id, const std::string & topic_name,
        const rclcpp::QoS & qos, const rclcpp::SubscriptionOptions & options)>;

  void declare_base_parameters()
  {
    declare_parameter<int>("check_period_ms", 100);
    declare_parameter<int>("status_publish_period_ms", 1000);
    declare_parameter<std::vector<std::string>>("monitor_ids", std::vector<std::string>{});
  }

  void read_base_parameters()
  {
    check_period_ms_ = get_parameter("check_period_ms").as_int();
    status_publish_period_ms_ = get_parameter("status_publish_period_ms").as_int();
    monitor_ids_ = get_parameter("monitor_ids").as_string_array();

    if (check_period_ms_ <= 0 || status_publish_period_ms_ <= 0) {
      throw std::runtime_error("check_period_ms and status_publish_period_ms must be > 0");
    }
    if (monitor_ids_.empty()) {
      throw std::runtime_error("monitor_ids must not be empty");
    }
  }

  void declare_monitor_parameters()
  {
    for (const auto & id : monitor_ids_) {
      declare_parameter<std::string>(id + ".node_name", "");
      declare_parameter<std::string>(id + ".topic_name", "");
      declare_parameter<std::string>(id + ".kind", "data");
      declare_parameter<std::string>(id + ".message_type", "");
      declare_parameter<std::string>(id + ".reliability", "reliable");
      declare_parameter<int>(id + ".deadline_ms", 0);
      declare_parameter<int>(id + ".liveliness_ms", 0);
      declare_parameter<int>(id + ".timeout_ms", 0);
    }
  }

  void read_monitor_parameters()
  {
    for (const auto & id : monitor_ids_) {
      MonitorConfig config;
      config.id = id;
      config.node_name = get_parameter(id + ".node_name").as_string();
      config.topic_name = get_parameter(id + ".topic_name").as_string();
      config.kind = get_parameter(id + ".kind").as_string();
      config.message_type = get_parameter(id + ".message_type").as_string();
      config.reliability = get_parameter(id + ".reliability").as_string();
      config.deadline_ms = get_parameter(id + ".deadline_ms").as_int();
      config.liveliness_ms = get_parameter(id + ".liveliness_ms").as_int();
      config.timeout_ms = get_parameter(id + ".timeout_ms").as_int();
      config.has_liveliness = config.liveliness_ms > 0;

      validate_monitor_config(config);
      monitors_.emplace(id, config);
    }
  }

  void validate_monitor_config(const MonitorConfig & config) const
  {
    if (config.node_name.empty() || config.topic_name.empty() || config.message_type.empty()) {
      throw std::runtime_error("monitor " + config.id + " has empty required fields");
    }
    if (config.kind != "heartbeat" && config.kind != "data") {
      throw std::runtime_error("monitor " + config.id + " has invalid kind");
    }
    if (config.reliability != "reliable" && config.reliability != "best_effort") {
      throw std::runtime_error("monitor " + config.id + " has invalid reliability");
    }
    if (config.deadline_ms < 0 || config.liveliness_ms < 0 || config.timeout_ms <= 0) {
      throw std::runtime_error("monitor " + config.id + " has invalid QoS/timeout values");
    }
    if (config.deadline_ms > 0 && config.timeout_ms <= config.deadline_ms) {
      throw std::runtime_error("monitor " + config.id + " timeout must be greater than deadline");
    }
    if (config.has_liveliness && config.timeout_ms <= config.liveliness_ms) {
      throw std::runtime_error("monitor " + config.id + " timeout must be greater than liveliness");
    }
  }

  rclcpp::QoS make_qos(const MonitorConfig & config) const
  {
    auto qos = rclcpp::QoS(rclcpp::KeepLast(10));
    if (config.reliability == "best_effort") qos.best_effort();
    else qos.reliable();
    if (config.deadline_ms > 0) qos.deadline(std::chrono::milliseconds(config.deadline_ms));
    if (config.has_liveliness) {
      qos.liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
         .liveliness_lease_duration(std::chrono::milliseconds(config.liveliness_ms));
    }
    return qos;
  }

  rclcpp::SubscriptionOptions make_subscription_options(const std::string & id)
  {
    rclcpp::SubscriptionOptions options;
    const auto & config = monitors_.at(id);

    if (config.deadline_ms > 0) {
      options.event_callbacks.deadline_callback = [this, id](rclcpp::QOSDeadlineRequestedInfo &) {
        publish_event_status(id, HealthStatus::STALE, HealthStatus::REASON_DEADLINE_MISSED, "deadline missed");
      };
    }
    options.event_callbacks.incompatible_qos_callback = [this, id](rclcpp::QOSRequestedIncompatibleQoSInfo &) {
      publish_event_status(id, HealthStatus::ERROR, HealthStatus::REASON_QOS_INCOMPATIBLE, "QoS incompatible");
    };
    if (config.has_liveliness) {
      options.event_callbacks.liveliness_callback = [this, id](rclcpp::QOSLivelinessChangedInfo & event) {
        if (event.not_alive_count_change > 0) {
          publish_event_status(id, HealthStatus::ERROR, HealthStatus::REASON_LIVELINESS_LOST, "liveliness lost");
        }
      };
    }
    return options;
  }

  void setup_publisher()
  {
    health_status_publisher_ = create_publisher<HealthStatus>("/health/status", rclcpp::QoS(rclcpp::KeepLast(10)).reliable());
  }

  void setup_management_subscription()
  {
    management_subscription_ = create_subscription<ManagementState>(
      "/management/state", rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(&HealthMonitorNode::handle_management_state, this, std::placeholders::_1));
  }

  void handle_management_state(const typename ManagementState::SharedPtr msg)
  {
    planned_inactive_reasons_.clear();
    const auto count = std::min(msg->planned_inactive_topics.size(), msg->planned_inactive_topic_reasons.size());
    for (size_t i = 0; i < count; ++i) {
      planned_inactive_reasons_[msg->planned_inactive_topics[i]] = management_reason_to_health_reason(msg->planned_inactive_topic_reasons[i]);
    }
  }

  uint8_t management_reason_to_health_reason(const std::string & reason) const
  {
    if (reason == "maintenance") return HealthStatus::REASON_MAINTENANCE;
    if (reason == "deregistered") return HealthStatus::REASON_DEREGISTERED;
    if (reason == "optional_disabled") return HealthStatus::REASON_OPTIONAL_DISABLED;
    if (reason == "mission_not_required") return HealthStatus::REASON_MISSION_NOT_REQUIRED;
    return HealthStatus::REASON_NONE;
  }

  bool topic_planned_inactive(const std::string & topic_name) const { return planned_inactive_reasons_.find(topic_name) != planned_inactive_reasons_.end(); }
  
  uint8_t planned_inactive_reason(const std::string & topic_name) const
  {
    const auto item = planned_inactive_reasons_.find(topic_name);
    if (item == planned_inactive_reasons_.end()) return HealthStatus::REASON_NONE;
    return item->second;
  }

  std::string planned_inactive_message(uint8_t reason) const
  {
    if (reason == HealthStatus::REASON_MAINTENANCE) return "maintenance";
    if (reason == HealthStatus::REASON_DEREGISTERED) return "deregistered";
    if (reason == HealthStatus::REASON_OPTIONAL_DISABLED) return "optional disabled";
    if (reason == HealthStatus::REASON_MISSION_NOT_REQUIRED) return "mission not required";
    return "planned inactive";
  }

  void setup_subscriptions()
  {
    for (const auto & id : monitor_ids_) {
      const auto & config = monitors_.at(id);
      const auto qos = make_qos(config);
      const auto options = make_subscription_options(id);

      auto factory = subscription_factories_.find(config.message_type);
      if (factory == subscription_factories_.end()) {
        throw std::runtime_error("unsupported message_type '" + config.message_type + "' for monitor " + id);
      }
      factory->second(id, config.topic_name, qos, options);
    }
  }

  void setup_timer()
  {
    check_timer_ = create_wall_timer(std::chrono::milliseconds(check_period_ms_), std::bind(&HealthMonitorNode::check_fallback_timeouts, this));
  }

  void handle_message(const std::string & id)
  {
    auto & config = monitors_.at(id);
    config.seen = true;
    config.last_update = now();

    if (topic_planned_inactive(config.topic_name)) {
      const uint8_t reason = planned_inactive_reason(config.topic_name);
      if (should_publish_periodic_status(config)) {
        publish_status(config, HealthStatus::INACTIVE, reason, planned_inactive_message(reason), 0.0F);
        config.last_status_publish = now();
      }
      return;
    }

    if (should_publish_periodic_status(config)) {
      publish_status(config, HealthStatus::OK, HealthStatus::REASON_NONE, "OK", 0.0F);
      config.last_status_publish = now();
    }
  }

  void check_fallback_timeouts()
  {
    for (const auto & id : monitor_ids_) {
      auto & config = monitors_.at(id);

      if (topic_planned_inactive(config.topic_name)) {
        const uint8_t reason = planned_inactive_reason(config.topic_name);
        if (should_publish_periodic_status(config)) {
          publish_status(config, HealthStatus::INACTIVE, reason, planned_inactive_message(reason), 0.0F);
          config.last_status_publish = now();
        }
        continue;
      }

      if (!config.seen) {
        if (should_publish_periodic_status(config)) {
          publish_status(config, HealthStatus::UNKNOWN, HealthStatus::REASON_HEARTBEAT_TIMEOUT, "waiting for first message", 0.0F);
          config.last_status_publish = now();
        }
        continue;
      }

      const double age_s = (now() - config.last_update).seconds();
      const double timeout_s = static_cast<double>(config.timeout_ms) / 1000.0;

      if (age_s > timeout_s) {
        if (should_publish_periodic_status(config)) {
          const uint8_t reason = config.kind == "heartbeat" ? HealthStatus::REASON_HEARTBEAT_TIMEOUT : HealthStatus::REASON_MESSAGE_TIMEOUT;
          publish_status(config, HealthStatus::STALE, reason, "message timeout fallback", static_cast<float>(age_s));
          config.last_status_publish = now();
        }
        continue;
      }

      if (should_publish_periodic_status(config)) {
        publish_status(config, HealthStatus::OK, HealthStatus::REASON_NONE, "OK", static_cast<float>(age_s));
        config.last_status_publish = now();
      }
    }
  }

  bool should_publish_periodic_status(const MonitorConfig & config) const
  {
    if (config.last_status_publish.nanoseconds() == 0) return true;
    const double age_s = (now() - config.last_status_publish).seconds();
    const double period_s = static_cast<double>(status_publish_period_ms_) / 1000.0;
    return age_s >= period_s;
  }

  void publish_event_status(const std::string & id, uint8_t status, uint8_t reason, const std::string & message)
  {
    if (topic_planned_inactive(monitors_.at(id).topic_name)) return;
    const auto & config = monitors_.at(id);
    float age_s = 0.0F;
    if (config.seen) age_s = static_cast<float>((now() - config.last_update).seconds());
    publish_status(config, status, reason, message, age_s);
  }

  void publish_status(const MonitorConfig & config, uint8_t status, uint8_t reason, const std::string & message, float last_update_age_s)
  {
    HealthStatus health;
    health.header.stamp = now();
    health.node_name = config.node_name;
    health.topic_name = config.topic_name;
    health.status = status;
    health.reason = reason;
    health.message = message;
    health.last_update_age_s = last_update_age_s;
    health_status_publisher_->publish(health);
  }

  int check_period_ms_;
  int status_publish_period_ms_;
  std::vector<std::string> monitor_ids_;
  std::unordered_map<std::string, MonitorConfig> monitors_;
  std::unordered_map<std::string, uint8_t> planned_inactive_reasons_;

  std::unordered_map<std::string, SubscriptionFactory> subscription_factories_;
  std::vector<rclcpp::SubscriptionBase::SharedPtr> generic_subscriptions_;

  typename rclcpp::Publisher<HealthStatus>::SharedPtr health_status_publisher_;
  rclcpp::TimerBase::SharedPtr check_timer_;
  typename rclcpp::Subscription<ManagementState>::SharedPtr management_subscription_;
};

}  // namespace health_monitor_pkg

#endif  // HEALTH_MONITOR_PKG__HEALTH_MONITOR_NODE_HPP_
