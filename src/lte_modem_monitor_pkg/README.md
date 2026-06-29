# 📡 ROS 2 Reusable LTE Modem Monitor Node

[![ROS 2](https://img.shields.io/badge/ROS_2-Humble%20%7C%20Iron%20%7C%20Jazzy-blue)](https://docs.ros.org/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-purple.svg)](https://en.cppreference.com/w/cpp/17)

A high-performance, reusable ROS 2 node designed to monitor LTE/4G modem connectivity using a hybrid approach: `nmcli` for interface state detection and Huawei HiLink HTTP API for signal quality metrics.

Unlike standard monitoring nodes that hardcode interface names and IP addresses directly into the source file, this package follows a strict **Template Engine + Implementation Plug-in** architecture. It uses **C++ virtual hooks** to allow any future project to reuse the core monitoring logic without modifying a single line of the engine.

---

## 🏗️ System Architecture

The node sits between the LTE hardware and the high-level system controllers, providing a unified cellular status feed without consuming high CPU loads.


### Data Flow Overview
1. **LTE Hardware (Blue):** Huawei USB stick or integrated modem providing network connectivity.
2. **nmcli (Orange):** Detects whether the LTE interface is in `connected` or `disconnected` state.
3. **LTE Monitor Node (Grey):** Periodically polls interface state via `nmcli` and signal quality via Huawei HTTP API, then publishes standardised status messages.
4. **System Status (Green):** Outputs unified `/network/lte/*` topics used by supervisors, dashboards, and safety controllers downstream.

---

## 📂 Repository Architecture (The Template Pattern)

This package is split into two distinct layers to enforce separation of concerns:

```
lte_modem_monitor/
├── include/
│   └── lte_modem_monitor/
│       └── lte_monitor_base.hpp     ← 🧠 THE REUSABLE ENGINE
├── src/
│   └── lte_monitor_node.cpp         ← 🚁 PROJECT-SPECIFIC PLUGIN
├── CMakeLists.txt
└── package.xml
```

| File | Role | Modify when... |
| :--- | :--- | :--- |
| `.hpp` (Header) | Contains the class `LteMonitorBase` with all publishers, timer, XML parsing, and virtual hooks. | **Never.** This is the locked engine. |
| `.cpp` (Source) | Inherits the base class and overrides hooks for your specific hardware. | **Adapting** to a different LTE interface name, stick IP, or data source. |

---

## ✨ Key Features & Academic Requirements

This package was explicitly designed to be a **reusable template** for any robotics project:

* **🧬 Virtual Hook Polymorphism:** Override `get_lte_interface()`, `fetch_huawei_signal()`, `fetch_huawei_plmn()`, `xml_value()`, and `on_poll()` to adapt to any LTE hardware without rewriting the core logic. This satisfies the **Open/Closed Principle** — open for extension, closed for modification.
* **🔀 Hybrid Data Source:** Combines `nmcli` (for reliable interface state) with Huawei HiLink HTTP API (for signal quality metrics), providing a more robust picture than either source alone.
* **🛡️ Hard DDS QoS Enforcement:** The heartbeat publisher uses `deadline` (2.5s) and `liveliness` (5s lease) QoS to enable supervisors to detect node failure the exact millisecond it happens.
* **🚫 Zero External Dependencies:** No YAML, no custom messages, no custom services — only standard `std_msgs`.

---

## 🚀 Quick Start

### 1. Build the Workspace
Open a terminal in your ROS 2 workspace root (e.g., `~/ros2_ws`):
```bash
colcon build --packages-select lte_modem_monitor
source install/setup.bash
```

### 2. Run the Node
```bash
ros2 run lte_modem_monitor lte_monitor_node
```

---

## ⚙️ Configuration Guide

Because this package follows a pure template architecture, **all configuration is done at compile time** by overriding virtual methods in the `.cpp` plug-in. No YAML file is needed.

### Available Virtual Hooks

| Hook | Purpose | Default Behaviour |
| :--- | :--- | :--- |
| `get_lte_interface()` | Returns the network interface name to monitor | `"enx001e101f0000"` |
| `fetch_device_status()` | Returns raw `nmcli device status` output | Runs `nmcli -t -f DEVICE,TYPE,STATE,CONNECTION device status` |
| `fetch_huawei_signal()` | Returns raw XML from signal endpoint | `curl http://192.168.8.1/api/device/signal` |
| `fetch_huawei_plmn()` | Returns raw XML from PLMN endpoint | `curl http://192.168.8.1/api/net/current-plmn` |
| `xml_value(xml, tag)` | Extracts a value from XML by tag name | Simple string search between `<tag>` and `</tag>` |
| `rat_text(rat)` | Converts numeric RAT code to human-readable string | `"7"` → `"LTE/4G"`, else `"RAT x"` |
| `dash(value)` | Replaces empty strings with `"--"` | Returns `"--"` if empty |
| `on_poll()` | Called after every publish cycle | No action |

---

## 🧑‍💻 Reusability Guide (For Future Projects)

This package proves true reusability. To use the LTE Monitor in a completely different project (e.g., an autonomous tractor with a different modem), **you do not modify the `.hpp` engine.**

### Step 1: Include the Template Engine
```cpp
#include "lte_modem_monitor/lte_monitor_base.hpp"
```

### Step 2: Create a New Implementation Plug-in
```cpp
class TractorLteMonitor : public lte_modem_monitor::LteMonitorBase
{
public:
  TractorLteMonitor() : LteMonitorBase("tractor_lte") {}

protected:
  // Different interface name on the tractor
  std::string get_lte_interface() const override
  {
    return "wwan0";
  }

  // Different stick IP on the tractor network
  std::string fetch_huawei_signal() override
  {
    return run_command("curl --connect-timeout 0.5 --max-time 1.0 -s http://192.168.1.1/api/device/signal");
  }

  std::string fetch_huawei_plmn() override
  {
    return run_command("curl --connect-timeout 0.5 --max-time 1.0 -s http://192.168.1.1/api/net/current-plmn");
  }

  // React to disconnection
  void on_poll() override
  {
    // Custom failsafe logic here
  }
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TractorLteMonitor>());
  rclcpp::shutdown();
  return 0;
}
```

That's it. **Zero modifications** to the original `.hpp` engine.

---

## 📡 Topic Interfaces

### Published Topics

| Topic | Type | Example Data | Description |
| :--- | :--- | :--- | :--- |
| `/network/lte/state` | `std_msgs/String` | `"CONNECTED"` / `"DISCONNECTED"` | Interface connection state from nmcli |
| `/network/lte/operator` | `std_msgs/String` | `"T-Mobile"` / `"--"` | Network operator name |
| `/network/lte/rat` | `std_msgs/String` | `"LTE/4G"` / `"--"` | Radio Access Technology |
| `/network/lte/rssi_dbm` | `std_msgs/String` | `"-75"` / `"--"` | Received Signal Strength Indicator |
| `/network/lte/rsrp_dbm` | `std_msgs/String` | `"-95"` / `"--"` | Reference Signal Received Power |
| `/network/lte/rsrq_db` | `std_msgs/String` | `"-12"` / `"--"` | Reference Signal Received Quality |
| `/network/lte/sinr_db` | `std_msgs/String` | `"15"` / `"--"` | Signal to Interference + Noise Ratio |
| `/network/lte/plmn` | `std_msgs/String` | `"310260"` / `"--"` | Public Land Mobile Network code |
| `/network/lte/heartbeat` | `std_msgs/String` | `"lte_monitor_node alive"` | Liveliness heartbeat for supervisor |

### Heartbeat QoS
| Setting | Value |
| :--- | :--- |
| **Reliability** | Reliable |
| **Deadline** | 2500 ms |
| **Liveliness** | Manual by Topic |
| **Lease Duration** | 5000 ms |

---

## 🎓 Design Patterns Used (Academic Reference)

| Pattern | Implementation | Benefit |
| :--- | :--- | :--- |
| **Template Method** | `LteMonitorBase` with virtual hooks | Reusable skeleton, project-specific filling |
| **Strategy Pattern** | `fetch_huawei_signal()`, `fetch_huawei_plmn()`, `xml_value()` | Swap data source and parsing without class change |
| **Observer Pattern** | DDS liveliness callbacks via `assert_liveliness()` | Reactive failure detection |

---

## 📄 License
MIT License. Free to use for academic and commercial projects.
