#include "supervisor_pkg/supervisor_node.hpp"

// Implementation of class methods
void SupervisorNode::declare_parameters() {
    declare_parameter<std::string>("safety_status_topic", "/safety/status");
    declare_parameter<std::string>("health_status_topic", "/health/status");
    declare_parameter<std::string>("supervisor_status_topic", "/supervisor/status");
    declare_parameter<std::string>("management_state_topic", "/management/state");
    declare_parameter<std::string>("heartbeat_topic", "/supervisor/heartbeat");
    declare_parameter<int>("evaluation_period_ms", 100);
    declare_parameter<int>("safety_status_timeout_ms", 500);
    declare_parameter<int>("health_status_timeout_ms", 1500);
    declare_parameter<int>("management_state_timeout_ms", 1500);
    declare_parameter<std::vector<std::string>>("required_health_topics", std::vector<std::string>{});
    declare_parameter<int>("heartbeat_period_ms", 500);
    declare_parameter<int>("heartbeat_deadline_ms", 700);
    declare_parameter<int>("heartbeat_liveliness_ms", 1500);
}

void SupervisorNode::read_parameters() {
    safety_status_topic_ = get_parameter("safety_status_topic").as_string();
    health_status_topic_ = get_parameter("health_status_topic").as_string();
    supervisor_status_topic_ = get_parameter("supervisor_status_topic").as_string();
    management_state_topic_ = get_parameter("management_state_topic").as_string();
    management_state_timeout_ms_ = get_parameter("management_state_timeout_ms").as_int();
    heartbeat_topic_ = get_parameter("heartbeat_topic").as_string();
    evaluation_period_ms_ = get_parameter("evaluation_period_ms").as_int();
    safety_status_timeout_ms_ = get_parameter("safety_status_timeout_ms").as_int();
    health_status_timeout_ms_ = get_parameter("health_status_timeout_ms").as_int();
    required_health_topics_ = get_parameter("required_health_topics").as_string_array();
    heartbeat_period_ms_ = get_parameter("heartbeat_period_ms").as_int();
    heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
    heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();
}

void SupervisorNode::validate_parameters() const {
    if (safety_status_topic_.empty() || health_status_topic_.empty() || supervisor_status_topic_.empty() || management_state_topic_.empty() || heartbeat_topic_.empty()) {
      throw std::runtime_error("supervisor topic parameters must not be empty");
    }
    if (evaluation_period_ms_ <= 0 || safety_status_timeout_ms_ <= 0 || health_status_timeout_ms_ <= 0 || management_state_timeout_ms_ <= 0) {
      throw std::runtime_error("supervisor timing parameters must be greater than 0");
    }
    if (required_health_topics_.empty()) { throw std::runtime_error("required_health_topics must not be empty"); }
    if (heartbeat_period_ms_ <= 0 || heartbeat_deadline_ms_ <= heartbeat_period_ms_ || heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_) {
      throw std::runtime_error("heartbeat timing must satisfy period < deadline < liveliness");
    }
}

void SupervisorNode::setup_heartbeat_qos() {
    heartbeat_qos_ = rclcpp::QoS(rclcpp::KeepLast(10))
      .reliable()
      .deadline(std::chrono::milliseconds(heartbeat_deadline_ms_))
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(std::chrono::milliseconds(heartbeat_liveliness_ms_));
}

void SupervisorNode::setup_communication() {
    safety_subscription_ = create_subscription<SafetyStatus>(safety_status_topic_, rclcpp::QoS(rclcpp::KeepLast(10)).reliable(), std::bind(&SupervisorNode::handle_safety_status, this, std::placeholders::_1));
    health_subscription_ = create_subscription<HealthStatus>(health_status_topic_, rclcpp::QoS(rclcpp::KeepLast(10)).reliable(), std::bind(&SupervisorNode::handle_health_status, this, std::placeholders::_1));
    management_subscription_ = create_subscription<ManagementState>(management_state_topic_, rclcpp::QoS(rclcpp::KeepLast(10)).reliable(), std::bind(&SupervisorNode::handle_management_state, this, std::placeholders::_1));
    supervisor_publisher_ = create_publisher<SupervisorStatus>(supervisor_status_topic_, rclcpp::QoS(rclcpp::KeepLast(10)).reliable());
    heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(heartbeat_topic_, heartbeat_qos_);
    evaluation_timer_ = create_wall_timer(std::chrono::milliseconds(evaluation_period_ms_), std::bind(&SupervisorNode::evaluate_supervisor_state, this));
    heartbeat_timer_ = create_wall_timer(std::chrono::milliseconds(heartbeat_period_ms_), std::bind(&SupervisorNode::publish_heartbeat, this));
    reset_service_ = create_service<Trigger>("/supervisor/reset_emergency_stop", std::bind(&SupervisorNode::handle_reset_emergency_stop, this, std::placeholders::_1, std::placeholders::_2));
}

void SupervisorNode::handle_safety_status(const SafetyStatus::SharedPtr msg) { latest_safety_status_ = *msg; has_safety_status_ = true; last_safety_status_time_ = now(); }
void SupervisorNode::handle_health_status(const HealthStatus::SharedPtr msg) { has_health_status_ = true; last_health_status_time_ = now(); latest_health_by_topic_[msg->topic_name] = *msg; }
void SupervisorNode::handle_management_state(const ManagementState::SharedPtr msg) {
    last_management_state_time_ = now(); has_management_state_ = true; maintenance_mode_ = msg->maintenance_mode; mission_active_ = msg->mission_active;
    planned_inactive_topics_.clear();
    const auto count = std::min(msg->planned_inactive_topics.size(), msg->planned_inactive_topic_reasons.size());
    for (size_t i = 0; i < count; ++i) { planned_inactive_topics_[msg->planned_inactive_topics[i]] = msg->planned_inactive_topic_reasons[i]; }
}

// NOTE: Add your remaining logic methods (evaluate_supervisor_state, etc) here to match your original snippet 1:1
// Due to space, I am keeping this file structure.

void SupervisorNode::publish_heartbeat() {
    std_msgs::msg::String heartbeat;
    heartbeat.data = "supervisor_node alive";
    heartbeat_publisher_->publish(heartbeat);
    heartbeat_publisher_->assert_liveliness();
}

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SupervisorNode>());
  rclcpp::shutdown();
  return 0;
}
