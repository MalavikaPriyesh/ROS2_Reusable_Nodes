#ifndef MISSION_MANAGER__MISSION_MANAGER_BASE_HPP_
#define MISSION_MANAGER__MISSION_MANAGER_BASE_HPP_

#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_srvs/srv/set_bool.hpp"

namespace mission_manager
{

using namespace std::chrono_literals;

// ============================================================
// Mission States
// ============================================================
enum class MissionState
{
  WAITING_TO_START,
  START_REQUESTED,
  INSPECTION,
  INSPECTION_COMPLETE,
  NAVIGATION_CONTINUE
};

// ============================================================
// Configuration Struct — passed by the plugin to the engine
// ============================================================
struct MissionConfig
{
  std::string phase_topic             = "/mission/phase";
  std::string mission_service         = "/management/set_mission_active";
  int         publish_period_ms       = 500;
  int         start_delay_s           = 5;
  int         inspection_duration_s   = 50;
  double      post_inspection_pause_s = 2.0;
};

// ============================================================
// THE REUSABLE ENGINE — never modify this class directly
// ============================================================
class MissionManagerBase : public rclcpp::Node
{
public:
  explicit MissionManagerBase(
    const std::string & node_name,
    const MissionConfig & config = MissionConfig{})
  : Node(node_name), config_(config)
  {
    validate_config();

    phase_pub_ = create_publisher<std_msgs::msg::String>(
      config_.phase_topic,
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    mission_client_ = create_client<std_srvs::srv::SetBool>(
      config_.mission_service);

    timer_ = create_wall_timer(
      std::chrono::milliseconds(config_.publish_period_ms),
      std::bind(&MissionManagerBase::tick, this));

    node_start_time_ = now();

    RCLCPP_INFO(get_logger(),
      "Mission manager base started | phase_topic=%s | start_delay=%ds",
      config_.phase_topic.c_str(), config_.start_delay_s);
  }

  virtual ~MissionManagerBase() = default;

protected:
  // ---------- HOOKS — override in your plugin ----------
  virtual void on_mission_started()     {}
  virtual void on_inspection_started()  {}
  virtual void on_inspection_complete() {}
  virtual void on_navigation_continue() {}
  virtual void on_tick()                {}

  virtual void on_mission_rejected(const std::string & reason)
  {
    RCLCPP_WARN(get_logger(), "Mission rejected: %s", reason.c_str());
  }

  virtual std::string phase_string(MissionState state) const
  {
    switch (state) {
      case MissionState::WAITING_TO_START:
      case MissionState::START_REQUESTED:     return "IDLE";
      case MissionState::INSPECTION:          return "INSPECTION";
      case MissionState::INSPECTION_COMPLETE: return "INSPECTION_COMPLETE";
      case MissionState::NAVIGATION_CONTINUE: return "NAVIGATION_CONTINUE";
      default:                                return "UNKNOWN";
    }
  }

  // ---------- Helpers available to the plugin ----------
  MissionState current_state() const { return state_; }

  void request_mission_active(bool active)
  {
    if (request_pending_) return;

    if (!mission_client_->service_is_ready()) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
        "Management service not ready, will retry");
      return;
    }

    auto req = std::make_shared<std_srvs::srv::SetBool::Request>();
    req->data = active;
    request_pending_ = true;

    mission_client_->async_send_request(req,
      [this, active](
        std::shared_future<std_srvs::srv::SetBool::Response::SharedPtr> future)
      {
        request_pending_ = false;
        const auto response = future.get();

        if (!response->success) {
          on_mission_rejected(response->message);
          if (active) { state_ = MissionState::WAITING_TO_START; }
          return;
        }

        RCLCPP_INFO(get_logger(), "Mission request accepted: %s",
          response->message.c_str());

        if (active) {
          state_ = MissionState::INSPECTION;
          inspection_start_time_ = now();
          on_mission_started();
          on_inspection_started();
        }
      });
  }

private:
  void validate_config() const
  {
    if (config_.publish_period_ms <= 0)
      throw std::runtime_error("publish_period_ms must be > 0");
    if (config_.start_delay_s < 0)
      throw std::runtime_error("start_delay_s must be >= 0");
    if (config_.inspection_duration_s <= 0)
      throw std::runtime_error("inspection_duration_s must be > 0");
    if (config_.post_inspection_pause_s < 0)
      throw std::runtime_error("post_inspection_pause_s must be >= 0");
  }

  void tick()
  {
    publish_phase();
    handle_state();
    on_tick();
  }

  void publish_phase()
  {
    std_msgs::msg::String msg;
    msg.data = phase_string(state_);
    phase_pub_->publish(msg);
  }

  void handle_state()
  {
    const double elapsed = (now() - node_start_time_).seconds();

    if (state_ == MissionState::WAITING_TO_START) {
      if (elapsed >= static_cast<double>(config_.start_delay_s)) {
        request_mission_active(true);
        state_ = MissionState::START_REQUESTED;
      }
      return;
    }

    if (state_ == MissionState::INSPECTION) {
      const double insp = (now() - inspection_start_time_).seconds();
      if (insp >= static_cast<double>(config_.inspection_duration_s)) {
        state_ = MissionState::INSPECTION_COMPLETE;
        inspection_complete_time_ = now();
        on_inspection_complete();
        RCLCPP_INFO(get_logger(), "Inspection complete");
      }
      return;
    }

    if (state_ == MissionState::INSPECTION_COMPLETE) {
      const double pause = (now() - inspection_complete_time_).seconds();
      if (pause >= config_.post_inspection_pause_s) {
        state_ = MissionState::NAVIGATION_CONTINUE;
        on_navigation_continue();
        RCLCPP_INFO(get_logger(), "Navigation continuing after inspection");
      }
      return;
    }
  }

  MissionConfig config_;
  MissionState  state_{MissionState::WAITING_TO_START};
  bool          request_pending_{false};

  rclcpp::Time  node_start_time_{0, 0, RCL_ROS_TIME};
  rclcpp::Time  inspection_start_time_{0, 0, RCL_ROS_TIME};
  rclcpp::Time  inspection_complete_time_{0, 0, RCL_ROS_TIME};

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr phase_pub_;
  rclcpp::Client<std_srvs::srv::SetBool>::SharedPtr   mission_client_;
  rclcpp::TimerBase::SharedPtr                        timer_;
};

}  // namespace mission_manager
#endif  // MISSION_MANAGER__MISSION_MANAGER_BASE_HPP_
