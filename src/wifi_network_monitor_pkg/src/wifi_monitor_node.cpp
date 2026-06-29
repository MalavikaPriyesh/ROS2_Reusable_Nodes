#include "wifi_network_monitor/wifi_monitor_base.hpp"

class MyWifiMonitor : public wifi_network_monitor::WifiMonitorBase
{
public:
  MyWifiMonitor() : WifiMonitorBase("my_wifi_monitor") {}

protected:
  // Example: add reaction on disconnect
  void on_publish(const std::string & state) override
  {
    if (state == "DISCONNECTED")
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 10000, "WiFi disconnected!");
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MyWifiMonitor>());
  rclcpp::shutdown();
  return 0;
}
