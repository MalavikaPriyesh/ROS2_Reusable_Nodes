#include "network_fusion/network_fusion_base.hpp"

class MyNetworkFusion : public network_fusion::NetworkFusionBase
{
public:
  MyNetworkFusion() : NetworkFusionBase("my_network_fusion")
  {
    // ⭐ Register all your links here. Add/remove freely – NO engine edits.
    // Arguments: (display_name, topic_name, priority [0=primary])
    add_link("WIFI",     "/network/wifi/state",     0);  // primary
    add_link("LTE",      "/network/lte/state",      1);  // first backup
    // add_link("STARLINK", "/network/starlink/state", 2);  // uncomment when ready
  }

protected:
  // Optional: stricter freshness window
  double freshness_timeout_s() const override { return 10.0; }

  // Optional: react when all links are lost
  void on_status(const std::string & status, const std::string & active) override
  {
    if (status == "NETWORK_UNHEALTHY") {
      RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 5000,
        "ALL NETWORK LINKS DOWN! Active=%s", active.c_str());
    }
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MyNetworkFusion>());
  rclcpp::shutdown();
  return 0;
}
