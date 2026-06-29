#include "at_modem_monitor/at_modem_monitor_base.hpp"

class MyAtModemMonitor : public at_modem_monitor::AtModemMonitorBase
{
public:
  MyAtModemMonitor() : AtModemMonitorBase("my_at_modem") {}

protected:
  // Override mock data for your test environment
  std::string generate_mock_data() override
  {
    return "CONNECTED,TestNetwork_AT+COPS,LTE/4G_AT^SYSINFOEX,-70dBm_AT+CSQ,-98dBm_AT^HCSQ,-10dB_AT^HCSQ,5dB_AT^HCSQ,310410_AT+COPS";
  }

  // When real serial is implemented, override this
  std::string read_real_at_data() override
  {
    // TODO: Implement serial AT commands here
    return "DISCONNECTED,--,--,--,--,--,--,--";
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MyAtModemMonitor>());
  rclcpp::shutdown();
  return 0;
}
