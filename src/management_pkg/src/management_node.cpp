#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "management_pkg/management_node.hpp"

using DroneManagementNode = management_pkg::ManagementNode<>;

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<DroneManagementNode>());
  rclcpp::shutdown();
  return 0;
}
