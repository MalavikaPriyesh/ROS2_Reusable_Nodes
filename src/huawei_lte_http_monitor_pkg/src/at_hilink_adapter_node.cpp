#include "huawei_lte_http_monitor/at_hilink_adapter_base.hpp"

class MyHuaweiMonitor : public huawei_lte_http_monitor::AtHilinkAdapterBase
{
public:
  MyHuaweiMonitor() : AtHilinkAdapterBase("my_huawei_monitor") {}

protected:
  // Example: override the base URL if stick is on a different IP
  std::string curl_get(const std::string & path) override
  {
    const std::string base = "http://192.168.8.1";  // change here if needed
    const std::string cmd = "curl --connect-timeout 0.3 --max-time 0.8 -s " + base + path;
    // reuse parent's run_command via protected member (we'll add a helper)
    // For simplicity we just call popen again
    std::array<char, 256> buf{};
    std::string result;
    FILE * pipe = popen(cmd.c_str(), "r");
    if (!pipe) return result;
    while (fgets(buf.data(), buf.size(), pipe) != nullptr) result += buf.data();
    pclose(pipe);
    return result;
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MyHuaweiMonitor>());
  rclcpp::shutdown();
  return 0;
}
