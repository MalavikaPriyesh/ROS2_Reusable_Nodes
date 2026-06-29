#ifndef SAFETY_FUSION_PKG__SAFETY_FUSION_NODE_HPP_
#define SAFETY_FUSION_PKG__SAFETY_FUSION_NODE_HPP_

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "drone_health_interfaces/msg/health_status.hpp"
#include "drone_health_interfaces/msg/safety_status.hpp"

namespace safety_fusion_pkg
{

template<
  typename HealthStatusMsg = drone_health_interfaces::msg::HealthStatus,
  typename SafetyStatusMsg = drone_health_interfaces::msg::SafetyStatus>
class SafetyFusionNode : public rclcpp::Node
{
public:
  using HealthStatus = HealthStatusMsg;
  using SafetyStatus = SafetyStatusMsg;

  SafetyFusionNode()
  : Node("safety_fusion_node")
  {
    declare_parameters();
    read_parameters();
    validate_parameters();
    setup_heartbeat_qos();
    setup_core_communication();

    RCLCPP_INFO(get_logger(), "Generic Safety Fusion Engine Initialized");
  }

protected:
  template<typename MsgT>
  void register_obstacle_subscription(
    const std::string & topic_name, const rclcpp::QoS & qos,
    std::function<float(const std::shared_ptr<MsgT>)> extractor)
  {
    obstacle_sub_ = this->template create_subscription<MsgT>(
      topic_name, qos,
      [this, extractor](const std::shared_ptr<MsgT> msg) {
        this->nearest_obstacle_m_ = extractor(msg);
        this->has_nearest_obstacle_ = true;
      });
  }

  template<typename MsgT>
  void register_velocity_subscription(
    const std::string & topic_name, const rclcpp::QoS & qos,
    std::function<float(const std::shared_ptr<MsgT>)> extractor)
  {
    velocity_sub_ = this->template create_subscription<MsgT>(
      topic_name, qos,
      [this, extractor](const std::shared_ptr<MsgT> msg) {
        this->speed_mps_ = extractor(msg);
        this->has_velocity_ = true;
      });
  }

  std::string nearest_obstacle_topic_;
  std::string velocity_topic_;

private:
  struct TopicHealth {
    uint8_t status{HealthStatus::UNKNOWN};
    uint8_t reason{HealthStatus::REASON_NONE};
    bool seen{false};
  };

  void declare_parameters()
  {
    declare_parameter<std::string>("nearest_obstacle_topic", "/lidar/nearest_obstacle");
    declare_parameter<std::string>("velocity_topic", "/vehicle/velocity");
    declare_parameter<std::string>("health_status_topic", "/health/status");
    declare_parameter<int>("health_status_timeout_ms", 1500);
    declare_parameter<std::string>("safety_status_topic", "/safety/status");
    declare_parameter<std::vector<std::string>>("required_health_topics", std::vector<std::string>{});
    declare_parameter<int>("evaluation_period_ms", 100);
    declare_parameter<double>("max_deceleration_mps2", 1.5);
    declare_parameter<double>("reaction_time_s", 0.2);
    declare_parameter<double>("safety_margin_m", 0.5);
    declare_parameter<std::string>("heartbeat_topic", "/safety_fusion/heartbeat");
    declare_parameter<int>("heartbeat_period_ms", 500);
    declare_parameter<int>("heartbeat_deadline_ms", 700);
    declare_parameter<int>("heartbeat_liveliness_ms", 1500);
  }

  void read_parameters()
  {
    nearest_obstacle_topic_ = get_parameter("nearest_obstacle_topic").as_string();
    velocity_topic_ = get_parameter("velocity_topic").as_string();
    health_status_topic_ = get_parameter("health_status_topic").as_string();
    health_status_timeout_ms_ = get_parameter("health_status_timeout_ms").as_int();
    safety_status_topic_ = get_parameter("safety_status_topic").as_string();
    required_health_topics_ = get_parameter("required_health_topics").as_string_array();
    evaluation_period_ms_ = get_parameter("evaluation_period_ms").as_int();
    max_deceleration_mps2_ = get_parameter("max_deceleration_mps2").as_double();
    reaction_time_s_ = get_parameter("reaction_time_s").as_double();
    safety_margin_m_ = get_parameter("safety_margin_m").as_double();
    heartbeat_topic_ = get_parameter("heartbeat_topic").as_string();
    heartbeat_period_ms_ = get_parameter("heartbeat_period_ms").as_int();
    heartbeat_deadline_ms_ = get_parameter("heartbeat_deadline_ms").as_int();
    heartbeat_liveliness_ms_ = get_parameter("heartbeat_liveliness_ms").as_int();
  }

  void validate_parameters() const
  {
    if (nearest_obstacle_topic_.empty() || velocity_topic_.empty() || health_status_topic_.empty() || safety_status_topic_.empty() || heartbeat_topic_.empty()) {
      throw std::runtime_error("Topic parameters must not be empty");
    }
    if (required_health_topics_.empty()) throw std::runtime_error("required_health_topics must not be empty");
    if (heartbeat_period_ms_ <= 0 || heartbeat_deadline_ms_ <= heartbeat_period_ms_ || heartbeat_liveliness_ms_ <= heartbeat_deadline_ms_) {
      throw std::runtime_error("Heartbeat timing must satisfy period < deadline < liveliness");
    }
    if (health_status_timeout_ms_ <= 0) throw std::runtime_error("health_status_timeout_ms must be > 0");
    if (evaluation_period_ms_ <= 0 || max_deceleration_mps2_ <= 0.0 || reaction_time_s_ < 0.0 || safety_margin_m_ < 0.0) {
      throw std::runtime_error("Invalid safety fusion parameter values");
    }
  }

  void setup_heartbeat_qos()
  {
    heartbeat_qos_ = rclcpp::QoS(rclcpp::KeepLast(10))
      .reliable()
      .deadline(std::chrono::milliseconds(heartbeat_deadline_ms_))
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(std::chrono::milliseconds(heartbeat_liveliness_ms_));
  }

  void setup_core_communication()
  {
    health_subscription_ = create_subscription<HealthStatus>(
      health_status_topic_, rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(&SafetyFusionNode::handle_health_status, this, std::placeholders::_1));

    safety_publisher_ = create_publisher<SafetyStatus>(
      safety_status_topic_, rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    heartbeat_publisher_ = create_publisher<std_msgs::msg::String>(
      heartbeat_topic_, heartbeat_qos_);

    heartbeat_timer_ = create_wall_timer(
      std::chrono::milliseconds(heartbeat_period_ms_), std::bind(&SafetyFusionNode::publish_heartbeat, this));

    evaluation_timer_ = create_wall_timer(
      std::chrono::milliseconds(evaluation_period_ms_), std::bind(&SafetyFusionNode::evaluate_safety, this));
  }

  void publish_heartbeat()
  {
    std_msgs::msg::String heartbeat;
    heartbeat.data = "safety_fusion_node alive";
    heartbeat_publisher_->publish(heartbeat);
    heartbeat_publisher_->assert_liveliness();
  }

  void handle_health_status(const typename HealthStatus::SharedPtr msg)
  {
    auto & health = topic_health_[msg->topic_name];
    health.status = msg->status;
    health.reason = msg->reason;
    health.seen = true;
    last_health_status_time_ = now();
    has_health_status_ = true;
  }

  bool health_status_fresh() const
  {
    if (!has_health_status_) return false;
    const double age_s = (now() - last_health_status_time_).seconds();
    return age_s <= (static_cast<double>(health_status_timeout_ms_) / 1000.0);
  }

  void evaluate_safety()
  {
    SafetyStatus status;
    status.header.stamp = now();
    status.header.frame_id = "base_link";

    status.nearest_obstacle_m = has_nearest_obstacle_ ? nearest_obstacle_m_ : 0.0F;
    status.speed_mps = has_velocity_ ? speed_mps_ : 0.0F;
    status.effective_deceleration_mps2 = static_cast<float>(max_deceleration_mps2_);
    status.safety_margin_m = static_cast<float>(safety_margin_m_);

    double required_clearance = 0.0;

    if (has_nearest_obstacle_ && has_velocity_ && input_values_valid()) {
      const double braking_distance = (speed_mps_ * speed_mps_) / (2.0 * max_deceleration_mps2_);
      const double reaction_distance = speed_mps_ * reaction_time_s_;
      required_clearance = braking_distance + reaction_distance + safety_margin_m_;

      status.braking_distance_m = static_cast<float>(braking_distance);
      status.reaction_distance_m = static_cast<float>(reaction_distance);
      status.required_clearance_m = static_cast<float>(required_clearance);
    }

    if (!has_nearest_obstacle_ || !has_velocity_ || !required_health_seen()) {
      status.state = SafetyStatus::UNKNOWN;
      status.reason = SafetyStatus::REASON_WAITING_FOR_INPUTS;
      safety_publisher_->publish(status);
      return;
    }

    if (!input_values_valid()) {
      status.state = SafetyStatus::UNKNOWN;
      status.reason = SafetyStatus::REASON_INVALID_INPUT;
      safety_publisher_->publish(status);
      return;
    }

    if (!health_status_fresh() || !required_health_ok()) {
      status.state = SafetyStatus::UNSAFE;
      status.reason = SafetyStatus::REASON_HEALTH_UNSAFE;
      safety_publisher_->publish(status);
      return;
    }

    if (nearest_obstacle_m_ <= required_clearance) {
      status.state = SafetyStatus::UNSAFE;
      status.reason = SafetyStatus::REASON_INSUFFICIENT_BRAKING_DISTANCE;
    } else {
      status.state = SafetyStatus::SAFE;
      status.reason = SafetyStatus::REASON_NONE;
    }
    safety_publisher_->publish(status);
  }

  bool input_values_valid() const {
    return std::isfinite(nearest_obstacle_m_) && std::isfinite(speed_mps_) &&
           nearest_obstacle_m_ >= 0.0F && speed_mps_ >= 0.0F;
  }

  bool required_health_seen() const {
    return std::all_of(required_health_topics_.begin(), required_health_topics_.end(),
      [this](const std::string & topic) {
        const auto item = topic_health_.find(topic);
        return item != topic_health_.end() && item->second.seen;
      });
  }

  bool required_health_ok() const {
    return std::all_of(required_health_topics_.begin(), required_health_topics_.end(),
      [this](const std::string & topic) {
        const auto item = topic_health_.find(topic);
        return item != topic_health_.end() && item->second.status == HealthStatus::OK;
      });
  }

  std::string health_status_topic_;
  std::string safety_status_topic_;
  std::vector<std::string> required_health_topics_;
  std::string heartbeat_topic_;

  int evaluation_period_ms_;
  double max_deceleration_mps2_;
  double reaction_time_s_;
  double safety_margin_m_;
  int heartbeat_period_ms_;
  int heartbeat_deadline_ms_;
  int heartbeat_liveliness_ms_;
  int health_status_timeout_ms_;

  bool has_health_status_{false};
  bool has_nearest_obstacle_{false};
  bool has_velocity_{false};
  float nearest_obstacle_m_{0.0F};
  float speed_mps_{0.0F};

  rclcpp::QoS heartbeat_qos_{rclcpp::KeepLast(10)};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr heartbeat_publisher_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp::Time last_health_status_time_{0, 0, RCL_ROS_TIME};

  std::unordered_map<std::string, TopicHealth> topic_health_;

  rclcpp::SubscriptionBase::SharedPtr obstacle_sub_;
  rclcpp::SubscriptionBase::SharedPtr velocity_sub_;
  typename rclcpp::Subscription<HealthStatus>::SharedPtr health_subscription_;
  typename rclcpp::Publisher<SafetyStatus>::SharedPtr safety_publisher_;
  rclcpp::TimerBase::SharedPtr evaluation_timer_;
};

} // namespace safety_fusion_pkg

#endif // SAFETY_FUSION_PKG__SAFETY_FUSION_NODE_HPP_
