#include <memory>
#include "rclcpp/rclcpp.hpp"

// Include the reusable template engine
#include "health_monitor_pkg/health_monitor_node.hpp"

// Include YOUR project-specific sensor message types
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/float32.hpp"
#include "std_msgs/msg/string.hpp"

/**
 * @brief Your Drone Project's specific implementation.
 * Inherits the template and plugs in the 5 sensors used by your drone.
 */
class DroneHealthMonitor : public health_monitor_pkg::HealthMonitorNode<>
{
protected:
  void register_user_message_types() override
  {
    register_message_type<std_msgs::msg::String>("string");
    register_message_type<std_msgs::msg::Float32>("float32");
    register_message_type<geometry_msgs::msg::TwistStamped>("twist_stamped");
    register_message_type<sensor_msgs::msg::LaserScan>("laser_scan");
    register_message_type<sensor_msgs::msg::Image>("image");
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<DroneHealthMonitor>());
  rclcpp::shutdown();
  return 0;
}
