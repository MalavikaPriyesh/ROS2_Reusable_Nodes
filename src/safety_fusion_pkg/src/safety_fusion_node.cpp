#include <memory>
#include <cmath>

#include "rclcpp/rclcpp.hpp"
#include "safety_fusion_pkg/safety_fusion_node.hpp"

// Specific messages used by YOUR drone project
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "std_msgs/msg/float32.hpp"

class DroneSafetyFusion : public safety_fusion_pkg::SafetyFusionNode<>
{
public:
  DroneSafetyFusion() : SafetyFusionNode<>()
  {
    // Inject logic to read obstacle distance from Float32
    register_obstacle_subscription<std_msgs::msg::Float32>(
      nearest_obstacle_topic_,
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      [](const std_msgs::msg::Float32::SharedPtr msg) -> float {
        return msg->data;
      });

    // Inject logic to read speed from TwistStamped
    register_velocity_subscription<geometry_msgs::msg::TwistStamped>(
      velocity_topic_,
      rclcpp::QoS(rclcpp::KeepLast(10)).best_effort(),
      [](const geometry_msgs::msg::TwistStamped::SharedPtr msg) -> float {
        const auto & linear = msg->twist.linear;
        return std::sqrt(linear.x * linear.x + linear.y * linear.y + linear.z * linear.z);
      });
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<DroneSafetyFusion>());
  rclcpp::shutdown();
  return 0;
}
