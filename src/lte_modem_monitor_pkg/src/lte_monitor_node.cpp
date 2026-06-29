#include "lte_modem_monitor/lte_monitor_base.hpp"

class MyLteMonitor : public lte_modem_monitor::LteMonitorBase
{
public:
  MyLteMonitor() : LteMonitorBase("my_lte_monitor") {}

protected:
  // Example: override if your LTE interface has a different name
  std::string get_lte_interface() const override
  {
    return "enx001e101f0000"; // change this for your hardware
  }

  // Example: override Huawei stick IP if different
  std::string fetch_huawei_signal() override
  {
    return run_command("curl --connect-timeout 0.3 --max-time 0.8 -s http://192.168.1.1/api/device/signal");
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MyLteMonitor>());
  rclcpp::shutdown();
  return 0;
}
