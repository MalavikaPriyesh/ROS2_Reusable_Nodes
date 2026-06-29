#ifndef MANAGEMENT_PKG__MANAGEMENT_NODE_HPP_
#define MANAGEMENT_PKG__MANAGEMENT_NODE_HPP_

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "rclcpp/rclcpp.hpp"
#include "std_srvs/srv/set_bool.hpp"
#include "std_msgs/msg/string.hpp"

// Use local custom message and services
#include "management_pkg/msg/management_state.hpp"
#include "management_pkg/srv/set_topic_inactive.hpp"
#include "management_pkg/srv/set_module_inactive.hpp"

// Use drone_health_interfaces only for external inputs
#include "drone_health_interfaces/msg/supervisor_status.hpp"

namespace management_pkg
{

template<
  typename ManagementStateMsg = management_pkg::msg::ManagementState,
  typename SupervisorStatusMsg = drone_health_interfaces::msg::SupervisorStatus,
  typename SetTopicInactiveSrv = management_pkg::srv::SetTopicInactive,
  typename SetModuleInactiveSrv = management_pkg::srv::SetModuleInactive>
class ManagementNode : public rclcpp::Node
{
public:
  using ManagementState = ManagementStateMsg;
  using SupervisorStatus = SupervisorStatusMsg;
  using SetTopicInactive = SetTopicInactiveSrv;
  using SetModuleInactive = SetModuleInactiveSrv;
  using SetBool = std_srvs::srv::SetBool;

  ManagementNode()
  : Node("management_node")
  {
    declare_module_parameters();
    read_module_parameters();

    state_publisher_ = this->template create_publisher<ManagementState>(
      "/management/state", rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    auto heartbeat_qos = rclcpp::QoS(rclcpp::KeepLast(10))
      .reliable()
      .deadline(std::chrono::milliseconds(700))
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(std::chrono::milliseconds(1500));

    heartbeat_publisher_ = this->template create_publisher<std_msgs::msg::String>(
      "/management/heartbeat", heartbeat_qos);

    supervisor_subscription_ = this->template create_subscription<SupervisorStatus>(
      supervisor_status_topic_, rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(&ManagementNode::handle_supervisor_status, this, std::placeholders::_1));

    maintenance_service_ = this->template create_service<SetBool>(
      "/management/set_maintenance_mode",
      std::bind(&ManagementNode::handle_set_maintenance_mode, this, std::placeholders::_1, std::placeholders::_2));

    mission_service_ = this->template create_service<SetBool>(
      "/management/set_mission_active",
      std::bind(&ManagementNode::handle_set_mission_active, this, std::placeholders::_1, std::placeholders::_2));

    topic_inactive_service_ = this->template create_service<SetTopicInactive>(
      "/management/set_topic_inactive",
      std::bind(&ManagementNode::handle_set_topic_inactive, this, std::placeholders::_1, std::placeholders::_2));

    module_inactive_service_ = this->template create_service<SetModuleInactive>(
      "/management/set_module_inactive",
      std::bind(&ManagementNode::handle_set_module_inactive, this, std::placeholders::_1, std::placeholders::_2));

    timer_ = create_wall_timer(
      std::chrono::milliseconds(200),
      std::bind(&ManagementNode::publish_state, this));

    RCLCPP_INFO(get_logger(), "Management node initialized");
  }

private:
  struct ModuleConfig
  {
    bool critical{false};
    std::vector<std::string> topics;
  };

  void handle_set_maintenance_mode(
    const typename SetBool::Request::SharedPtr request,
    typename SetBool::Response::SharedPtr response)
  {
    if (request->data && mission_active_) {
      response->success = false;
      response->message = "cannot enable maintenance mode during active mission";
      return;
    }

    maintenance_mode_ = request->data;
    response->success = true;
    response->message = maintenance_mode_ ? "maintenance mode enabled" : "maintenance mode disabled";
    publish_state();
  }

  void handle_set_mission_active(
    const typename SetBool::Request::SharedPtr request,
    typename SetBool::Response::SharedPtr response)
  {
    if (request->data) {
      if (maintenance_mode_) {
        response->success = false;
        response->message = "cannot start mission while maintenance mode is active";
        return;
      }
      if (has_planned_inactive_critical_item()) {
        response->success = false;
        response->message = "cannot start mission while critical module/topic is planned inactive";
        return;
      }
      if (!supervisor_allows_mission_start()) {
        response->success = false;
        response->message = "cannot start mission because supervisor does not allow command";
        return;
      }
    }

    mission_active_ = request->data;
    response->success = true;
    response->message = mission_active_ ? "mission active enabled" : "mission active disabled";
    publish_state();
  }

  void handle_set_topic_inactive(
    const typename SetTopicInactive::Request::SharedPtr request,
    typename SetTopicInactive::Response::SharedPtr response)
  {
    if (request->topic_name.empty()) {
      response->success = false;
      response->message = "topic_name must not be empty";
      return;
    }

    if (request->inactive) {
      if (!inactive_reason_valid(request->reason)) {
        response->success = false;
        response->message = "reason must be maintenance, deregistered, optional_disabled, or mission_not_required";
        return;
      }
      if (mission_active_ && topic_is_critical(request->topic_name)) {
        response->success = false;
        response->message = "cannot mark critical topic inactive during active mission: " + request->topic_name;
        return;
      }

      planned_inactive_topics_[request->topic_name] = request->reason;
      response->success = true;
      response->message = "topic marked planned inactive: " + request->topic_name + " - " + request->reason;
      publish_state();
      return;
    }

    const auto removed = planned_inactive_topics_.erase(request->topic_name);
    response->success = true;
    response->message = (removed > 0) ? "topic restored active: " + request->topic_name : "topic was not planned inactive: " + request->topic_name;
    publish_state();
  }

  void handle_set_module_inactive(
    const typename SetModuleInactive::Request::SharedPtr request,
    typename SetModuleInactive::Response::SharedPtr response)
  {
    if (request->module_name.empty()) {
      response->success = false;
      response->message = "module_name must not be empty";
      return;
    }
    if (!module_name_valid(request->module_name)) {
      response->success = false;
      response->message = "module_name is not configured: " + request->module_name;
      return;
    }

    if (request->inactive) {
      if (!inactive_reason_valid(request->reason)) {
        response->success = false;
        response->message = "reason must be maintenance, deregistered, optional_disabled, or mission_not_required";
        return;
      }
      if (mission_active_ && module_is_critical(request->module_name)) {
        response->success = false;
        response->message = "cannot mark critical module inactive during active mission: " + request->module_name;
        return;
      }

      planned_inactive_modules_[request->module_name] = request->reason;
      response->success = true;
      response->message = "module marked planned inactive: " + request->module_name + " - " + request->reason;
      publish_state();
      return;
    }

    const auto removed = planned_inactive_modules_.erase(request->module_name);
    response->success = true;
    response->message = (removed > 0) ? "module restored active: " + request->module_name : "module was not planned inactive: " + request->module_name;
    publish_state();
  }

  bool inactive_reason_valid(const std::string & reason) const
  {
    return reason == "maintenance" || reason == "deregistered" ||
           reason == "optional_disabled" || reason == "mission_not_required";
  }

  void declare_module_parameters()
  {
    declare_parameter<std::string>("supervisor_status_topic", "/supervisor/status");
    declare_parameter<int>("supervisor_status_timeout_ms", 1000);
    declare_parameter<std::vector<std::string>>("module_ids", std::vector<std::string>{});
  }

  void read_module_parameters()
  {
    supervisor_status_topic_ = get_parameter("supervisor_status_topic").as_string();
    supervisor_status_timeout_ms_ = get_parameter("supervisor_status_timeout_ms").as_int();

    if (supervisor_status_topic_.empty()) {
      throw std::runtime_error("supervisor_status_topic must not be empty");
    }
    if (supervisor_status_timeout_ms_ <= 0) {
      throw std::runtime_error("supervisor_status_timeout_ms must be greater than 0");
    }

    module_ids_ = get_parameter("module_ids").as_string_array();
    if (module_ids_.empty()) {
      throw std::runtime_error("module_ids must not be empty");
    }

    for (const auto & module_id : module_ids_) {
      declare_parameter<bool>(module_id + ".critical", false);
      declare_parameter<std::vector<std::string>>(module_id + ".topics", std::vector<std::string>{});

      ModuleConfig config;
      config.critical = get_parameter(module_id + ".critical").as_bool();
      config.topics = get_parameter(module_id + ".topics").as_string_array();

      if (config.topics.empty()) {
        throw std::runtime_error("module " + module_id + " must have at least one topic");
      }
      modules_.emplace(module_id, config);
    }
  }

  bool module_name_valid(const std::string & module_name) const { return modules_.find(module_name) != modules_.end(); }
  bool module_is_critical(const std::string & module_name) const { auto it = modules_.find(module_name); return it != modules_.end() && it->second.critical; }

  bool topic_is_critical(const std::string & topic_name) const
  {
    for (const auto & item : modules_) {
      if (!item.second.critical) continue;
      for (const auto & topic : item.second.topics) {
        if (topic == topic_name) return true;
      }
    }
    return false;
  }

  bool has_planned_inactive_critical_item() const
  {
    for (const auto & item : planned_inactive_modules_) {
      if (module_is_critical(item.first)) return true;
    }
    for (const auto & item : planned_inactive_topics_) {
      if (topic_is_critical(item.first)) return true;
    }
    return false;
  }

  void handle_supervisor_status(const typename SupervisorStatus::SharedPtr msg)
  {
    latest_supervisor_status_ = *msg;
    last_supervisor_status_time_ = now();
    has_supervisor_status_ = true;
  }

  bool supervisor_status_fresh()
  {
    if (!has_supervisor_status_) return false;
    const double age_s = (now() - last_supervisor_status_time_).seconds();
    return age_s <= (static_cast<double>(supervisor_status_timeout_ms_) / 1000.0);
  }

  bool supervisor_allows_mission_start()
  {
    if (!supervisor_status_fresh()) return false;
    return latest_supervisor_status_.mode == SupervisorStatusMsg::NORMAL && latest_supervisor_status_.command_allowed;
  }

  void publish_state()
  {
    ManagementState state;
    state.header.stamp = now();
    state.header.frame_id = "base_link";
    state.maintenance_mode = maintenance_mode_;
    state.mission_active = mission_active_;

    for (const auto & item : planned_inactive_topics_) {
      state.planned_inactive_topics.push_back(item.first);
      state.planned_inactive_topic_reasons.push_back(item.second);
    }

    for (const auto & item : planned_inactive_modules_) {
      state.planned_inactive_modules.push_back(item.first);
      state.planned_inactive_module_reasons.push_back(item.second);

      const auto module = modules_.find(item.first);
      if (module == modules_.end()) continue;

      for (const auto & topic : module->second.topics) {
        bool already_added = false;
        for (const auto & existing_topic : state.planned_inactive_topics) {
          if (existing_topic == topic) { already_added = true; break; }
        }
        if (!already_added) {
          state.planned_inactive_topics.push_back(topic);
          state.planned_inactive_topic_reasons.push_back(item.second);
        }
      }
    }

    state.reason = ManagementState::REASON_NONE;
    state.message = "management state normal";

    if (maintenance_mode_) {
      state.reason = ManagementState::REASON_MAINTENANCE_MODE;
      state.message = "maintenance mode active";
    } else if (!planned_inactive_modules_.empty()) {
      state.reason = ManagementState::REASON_PLANNED_INACTIVE;
      if (planned_inactive_modules_.size() == 1) {
        const auto & item = *planned_inactive_modules_.begin();
        state.message = "planned inactive module: " + item.first + " - " + item.second;
      } else {
        state.message = std::to_string(planned_inactive_modules_.size()) + " planned inactive modules";
      }
    } else if (!planned_inactive_topics_.empty()) {
      state.reason = ManagementState::REASON_PLANNED_INACTIVE;
      if (planned_inactive_topics_.size() == 1) {
        const auto & item = *planned_inactive_topics_.begin();
        state.message = "planned inactive: " + item.first + " - " + item.second;
      } else {
        state.message = std::to_string(planned_inactive_topics_.size()) + " planned inactive topics";
      }
    }

    std_msgs::msg::String heartbeat;
    heartbeat.data = "management_node alive";
    heartbeat_publisher_->publish(heartbeat);

    if (!heartbeat_publisher_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "Failed to assert management node liveliness");
    }

    state_publisher_->publish(state);
  }

  bool maintenance_mode_{false};
  bool mission_active_{false};
  int supervisor_status_timeout_ms_;

  bool has_supervisor_status_{false};
  SupervisorStatusMsg latest_supervisor_status_;
  rclcpp::Time last_supervisor_status_time_{0, 0, RCL_ROS_TIME};

  std::string supervisor_status_topic_;
  std::vector<std::string> module_ids_;
  std::unordered_map<std::string, ModuleConfig> modules_;

  std::unordered_map<std::string, std::string> planned_inactive_topics_;
  std::unordered_map<std::string, std::string> planned_inactive_modules_;

  typename rclcpp::Publisher<ManagementState>::SharedPtr state_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
  typename rclcpp::Subscription<SupervisorStatus>::SharedPtr supervisor_subscription_;
  typename rclcpp::Service<SetBool>::SharedPtr maintenance_service_;
  typename rclcpp::Service<SetBool>::SharedPtr mission_service_;
  typename rclcpp::Service<SetTopicInactive>::SharedPtr topic_inactive_service_;
  typename rclcpp::Service<SetModuleInactive>::SharedPtr module_inactive_service_;
  rclcpp::TimerBase::SharedPtr timer_;
};

} // namespace management_pkg

#endif // MANAGEMENT_PKG__MANAGEMENT_NODE_HPP_
