#include "mission_manager/mission_manager_base.hpp"

class FlyingRobotMissionManager : public mission_manager::MissionManagerBase
{
public:
  FlyingRobotMissionManager()
  : MissionManagerBase(
      "flying_robot_mission_manager",
      build_config())
  {}

protected:
  void on_mission_started() override
  {
    RCLCPP_INFO(get_logger(), "Mission started — flying robot beginning inspection");
  }

  void on_inspection_started() override
  {
    RCLCPP_INFO(get_logger(), "Inspection started — camera active");
  }

  void on_inspection_complete() override
  {
    RCLCPP_INFO(get_logger(), "Inspection complete — camera deactivating");
  }

  void on_navigation_continue() override
  {
    RCLCPP_INFO(get_logger(), "Navigation continuing after inspection");
  }

  void on_mission_rejected(const std::string & reason) override
  {
    RCLCPP_WARN(get_logger(),
      "Mission rejected by management node: %s", reason.c_str());
  }

private:
  static mission_manager::MissionConfig build_config()
  {
    mission_manager::MissionConfig cfg;
    cfg.phase_topic             = "/mission/phase";
    cfg.mission_service         = "/management/set_mission_active";
    cfg.publish_period_ms       = 500;
    cfg.start_delay_s           = 5;
    cfg.inspection_duration_s   = 50;
    cfg.post_inspection_pause_s = 2.0;
    return cfg;
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FlyingRobotMissionManager>());
  rclcpp::shutdown();
  return 0;
}
