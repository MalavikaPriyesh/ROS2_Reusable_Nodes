# 🌐 huawei_lte_http_monitor – Reusable Huawei LTE Monitor for ROS 2

[![ROS 2](https://img.shields.io/badge/ROS_2-Humble%20%7C%20Iron%20%7C%20Jazzy-blue)](https://docs.ros.org/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-purple.svg)](https://en.cppreference.com/w/cpp/17)

A lightweight, reusable ROS 2 node that monitors Huawei LTE sticks using their built-in HiLink HTTP API (`curl`). The node translates XML responses into standard AT-formatted strings and publishes them as ROS 2 topics.

Designed with a strict **Template Engine + Implementation Plug-in** pattern to allow any project to reuse the core logic without modifying the engine.

---

## 🏗️ System Architecture

The node bridges the HiLink HTTP interface and the ROS 2 ecosystem, providing a unified LTE status feed to supervisors and safety monitors.

### Data Flow Overview

1. **Huawei LTE Stick (HiLink)** – Hosts a local web server at `192.168.8.1` with XML endpoints.
2. **LTE Monitor Node** – Periodically fetches `/api/device/signal` and `/api/net/current-plmn`, extracts values, and publishes them as AT-formatted strings.
3. **Downstream Nodes** – Supervisor, Dashboard, and Mission Manager consume the topics for network awareness.

---

## 📂 Repository Architecture (The Template Pattern)

This package is split into two distinct layers to enforce separation of concerns:

```text
huawei_lte_http_monitor/
├── include/
│   └── huawei_lte_http_monitor/
│       └── at_hilink_adapter_base.hpp   ← 🧠 THE REUSABLE ENGINE
├── src/
│   └── at_hilink_adapter_node.cpp       ← 🚁 PROJECT-SPECIFIC PLUGIN
├── CMakeLists.txt
├── package.xml
└── README.md
```

| File | Role | Modify when... |
| :--- | :--- | :--- |
| `at_hilink_adapter_base.hpp` | Contains the class `AtHilinkAdapterBase` with all publishers, timer, XML extraction, and virtual hooks. | **Never.** This is the locked engine. |
| `at_hilink_adapter_node.cpp` | Inherits from the base class and overrides hooks to change the stick IP, API paths, or parsing logic. | **Every time** you adapt the node to a different HiLink device or network environment. |

---

## ✨ Key Features

- **🧬 Virtual Hooks for Complete Customisation:** Override `fetch_signal()`, `fetch_plmn()`, `extract_xml()`, `rat_to_text()`, `value_or_dash()`, and `on_poll()` to adapt to any HiLink-compatible stick without touching the engine. This satisfies the **Open/Closed Principle**.
- **🏭 Factory-like Registration:** No type registries – just override the hooks you need.
- **📡 AT-Formatted Output:** All topics follow the AT command syntax (e.g., `"AT+CSQ -> -75"`) for seamless integration with legacy LTE parsers.
- **🛡️ Hard DDS QoS Enforcement:** The heartbeat publisher uses `liveliness` and `deadline` QoS to enable supervisors to detect node failure immediately.
- **🚫 Zero External Dependencies:** No YAML, no custom messages, no custom services – only standard `std_msgs`.

---

## 🚀 Quick Start

### 1. Build the Package

```bash
cd ~/ros2_ws
colcon build --packages-select huawei_lte_http_monitor
source install/setup.bash
```

### 2. Run the Node

```bash
ros2 run huawei_lte_http_monitor at_hilink_adapter_node
```

> Ensure your PC is connected to the Huawei stick network via Wi-Fi or Ethernet before running.

---

## ⚙️ Configuration Guide

Because this package follows a pure template architecture, all configuration is done at compile time by overriding virtual methods in the plugin file. The following table lists all available hooks.

| Hook | Purpose | Default Behaviour |
| :--- | :--- | :--- |
| `fetch_signal()` | Returns raw XML from `/api/device/signal` | `curl http://192.168.8.1/api/device/signal` |
| `fetch_plmn()` | Returns raw XML from `/api/net/current-plmn` | `curl http://192.168.8.1/api/net/current-plmn` |
| `extract_xml(xml, tag)` | Extracts value of an XML tag | Simple string search between open and close tags |
| `rat_to_text(rat)` | Converts numeric RAT code to human-readable string | Maps `"7"` to `"LTE/4G"`, else returns `"RAT x"` |
| `value_or_dash(val)` | Replaces empty strings with `"--"` | Returns `"--"` if empty, else returns value |
| `on_poll()` | Called after every poll cycle | No action |

### Example: Changing the Stick IP Address

```cpp
class MyHuaweiMonitor : public huawei_lte_http_monitor::AtHilinkAdapterBase
{
protected:
  std::string curl_get(const std::string & path) override
  {
    const std::string base = "http://192.168.1.1";
    const std::string cmd =
      "curl --connect-timeout 0.3 --max-time 0.8 -s " + base + path;
    return run_command(cmd);
  }
};
```

---

## 🧑‍💻 Reusability Guide (For Future Projects)

To use the Huawei LTE Monitor in a different robot project, **you do not modify the `.hpp` engine.**

### Step 1: Include the Template Engine

```cpp
#include "huawei_lte_http_monitor/at_hilink_adapter_base.hpp"
```

### Step 2: Create a New Implementation Plug-in

```cpp
class RobotLteMonitor : public huawei_lte_http_monitor::AtHilinkAdapterBase
{
public:
  RobotLteMonitor() : AtHilinkAdapterBase("robot_lte") {}

protected:
  std::string fetch_signal() override
  {
    return curl_get("/api/monitoring/status");
  }

  void on_poll() override
  {
    RCLCPP_DEBUG(get_logger(), "Poll complete");
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<RobotLteMonitor>());
  rclcpp::shutdown();
  return 0;
}
```

### Step 3: Build and Run

```bash
colcon build --packages-select huawei_lte_http_monitor
source install/setup.bash
ros2 run huawei_lte_http_monitor at_hilink_adapter_node
```

That is it. **Zero modifications** to the original `.hpp` engine.

---

## 📡 Topic Interfaces

### Published Topics

| Topic | Type | Example Data | Description |
| :--- | :--- | :--- | :--- |
| `/network/at_hilink/state` | `std_msgs/String` | `"OK_FROM_AT"` | Is the stick reachable? |
| `/network/at_hilink/operator` | `std_msgs/String` | `"AT+COPS? -> T-Mobile"` | Network operator name |
| `/network/at_hilink/rat` | `std_msgs/String` | `"AT^SYSINFOEX -> LTE/4G"` | Radio Access Technology |
| `/network/at_hilink/rssi_dbm` | `std_msgs/String` | `"AT+CSQ -> -75"` | Received Signal Strength |
| `/network/at_hilink/rsrp_dbm` | `std_msgs/String` | `"AT^HCSQ? -> -95"` | Reference Signal Received Power |
| `/network/at_hilink/rsrq_db` | `std_msgs/String` | `"AT^HCSQ? -> -12"` | Reference Signal Received Quality |
| `/network/at_hilink/sinr_db` | `std_msgs/String` | `"AT^HCSQ? -> 15"` | Signal to Interference and Noise Ratio |
| `/network/at_hilink/plmn` | `std_msgs/String` | `"AT+COPS? -> 310260"` | Public Land Mobile Network code |
| `/network/at_hilink/at_summary` | `std_msgs/String` | `"AT=OK; AT+CSQ=-75; ..."` | All data combined in one string |
| `/network/at_hilink/heartbeat` | `std_msgs/String` | `"at_hilink_adapter_node alive"` | Liveliness heartbeat for supervisor |

### Heartbeat QoS Settings

| Setting | Value |
| :--- | :--- |
| Reliability | Reliable |
| Deadline | 2500 ms |
| Liveliness | Manual by Topic |
| Lease Duration | 5000 ms |

---

## 🧪 Quick Test

Open two terminals to verify everything works:

```bash
# Terminal 1 — run the node
ros2 run huawei_lte_http_monitor at_hilink_adapter_node

# Terminal 2 — check state topic
ros2 topic echo /network/at_hilink/state

# Terminal 2 — check summary topic
ros2 topic echo /network/at_hilink/at_summary
```

You should see `data: 'OK_FROM_AT'` every 2 seconds if the stick is connected.

---

## 🗂️ Related Modules

This package is part of the Flying Robot modular system:

| Package | Purpose |
| :--- | :--- |
| `wifi_network_monitor` | Wi-Fi monitoring via nmcli |
| `lte_modem_monitor` | LTE interface monitoring via nmcli |
| `at_modem_monitor` | LTE modem monitoring via AT commands |
| `network_fusion` | Fuses all network link states into one status |
| `health_monitor` | Node and topic liveness checks |
| `supervisor_node` | Central decision maker |

---

## 🎓 Design Patterns Used

| Pattern | Implementation | Benefit |
| :--- | :--- | :--- |
| **Template Method** | `AtHilinkAdapterBase` with virtual hooks | Reusable skeleton, project-specific filling |
| **Strategy Pattern** | `fetch_signal()`, `fetch_plmn()`, `extract_xml()` | Swap data source and parsing without class change |
| **Observer Pattern** | DDS liveliness callbacks via `assert_liveliness()` | Reactive failure detection |

---

## 📄 License

MIT License. Free to use for academic and commercial projects.
