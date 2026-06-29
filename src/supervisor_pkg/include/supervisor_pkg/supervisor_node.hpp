#ifndef SUPERVISOR_PKG__SUPERVISOR_NODE_HPP_
#define SUPERVISOR_PKG__SUPERVISOR_NODE_HPP_

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "drone_health_interfaces/msg/health_status.hpp"
#include "drone_health_interfaces/msg/safety_status.hpp"
#include "supervisor_pkg/msg/supervisor_status.hpp"
#include "drone_health_interfaces/msg/management_state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_srvs/srv/trigger.hpp"

class SupervisorNode : public rclcpp::Node
{
public:
  SupervisorNode() : Node("supervisor_node")
  {
    declare_parameters();
    read_parameters();
    validate_parameters();
    setup_heartbeat_qos();
    setup_communication();
    RCLCPP_INFO(get_logger(), "Supervisor node started");
  }

private:
  using HealthStatus = drone_health_interfaces::msg::HealthStatus;
  using SafetyStatus = drone_health_interfaces::msg::SafetyStatus;
  using SupervisorStatus = supervisor_pkg::msg::SupervisorStatus;
  using ManagementState = drone_health_interfaces::msg::ManagementState;
  using Trigger = std_srvs::srv::Trigger;

  void declare_parameters();
  void read_parameters();
  void validate_parameters() const;
  void setup_heartbeat_qos();
  void setup_communication();
  void handle_safety_status(const SafetyStatus::SharedPtr msg);
  void handle_health_status(const HealthStatus::SharedPtr msg);
  void handle_management_state(const ManagementState::SharedPtr msg);
  void evaluate_supervisor_state();
  bool safety_status_fresh() const;
  bool health_status_fresh() const;
  bool management_state_fresh() const;
  bool required_health_topics_ok(std::string & failure_message) const;
  bool planned_inactive_required_topics(std::string & message) const;
  void handle_reset_emergency_stop(const std::shared_ptr<Trigger::Request>, std::shared_ptr<Trigger::Response> response);
  void publish_heartbeat();

  std::string safety_status_topic_, health_status_topic_, supervisor_status_topic_, heartbeat_topic_, management_state_topic_;
  int evaluation_period_ms_, safety_status_timeout_ms_, health_status_timeout_ms_, management_state_timeout_ms_{1500};
  std::vector<std::string> required_health_topics_;
  int heartbeat_period_ms_, heartbeat_deadline_ms_, heartbeat_liveliness_ms_;
  bool has_safety_status_{false}, has_health_status_{false}, emergency_stop_latched_{false}, maintenance_mode_{false}, mission_active_{false}, has_management_state_{false};
  SafetyStatus latest_safety_status_;
  std::unordered_map<std::string, HealthStatus> latest_health_by_topic_;
  std::unordered_map<std::string, std::string> planned_inactive_topics_;
  rclcpp::Time last_safety_status_time_{0, 0, RCL_ROS_TIME}, last_health_status_time_{0, 0, RCL_ROS_TIME}, last_management_state_time_{0, 0, RCL_ROS_TIME};
  rclcpp::QoS heartbeat_qos_{rclcpp::KeepLast(10)};
  rclcpp::Subscription<SafetyStatus>::SharedPtr safety_subscription_;
  rclcpp::Subscription<HealthStatus>::SharedPtr health_subscription_;
  rclcpp::Publisher<SupervisorStatus>::SharedPtr supervisor_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
  rclcpp::TimerBase::SharedPtr evaluation_timer_, heartbeat_timer_;
  rclcpp::Service<Trigger>::SharedPtr reset_service_;
  rclcpp::Subscription<ManagementState>::SharedPtr management_subscription_;
};

#endif // SUPERVISOR_PKG__SUPERVISOR_NODE_HPP_
