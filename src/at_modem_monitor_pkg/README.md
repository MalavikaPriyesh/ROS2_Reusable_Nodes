# 📟 ROS 2 Reusable AT Modem Monitor Node

[![ROS 2](https://img.shields.io/badge/ROS_2-Humble%20%7C%20Iron%20%7C%20Jazzy-blue)](https://docs.ros.org/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-purple.svg)](https://en.cppreference.com/w/cpp/17)

A high-performance, reusable ROS 2 node designed to monitor LTE/5G modems using standard AT commands over a serial interface. Includes a built-in **mock mode** for development and testing without physical hardware.

Unlike standard monitoring nodes that hardcode serial communication directly into the source file, this package follows a strict **Template Engine + Implementation Plug-in** architecture. It uses **C++ virtual hooks** to allow any future project to reuse the core monitoring logic without modifying a single line of the engine.

---

## 🏗️ System Architecture

The node sits between the serial modem hardware and the high-level system controllers, providing a unified cellular status feed. When no physical modem is available, it operates in mock mode for dashboard development and integration testing.



### Data Flow Overview
1. **LTE/5G Modem (Blue):** Integrated modem or USB stick connected via serial (`/dev/ttyUSB0`).
2. **AT Modem Monitor Node (Grey):** Periodically sends AT commands (or generates mock data), parses responses, and publishes standardised status messages.
3. **System Status (Green):** Outputs unified `/network/at_lte/*` topics used by supervisors, dashboards, and safety controllers downstream.

### Why This Node Exists

The professor noted that future flying robot hardware will use **integrated LTE/5G modems** that do not have a web interface like the Huawei USB stick. These modems only support AT command communication over serial. This node provides the foundation for that transition.

> **Current Status:** Mock mode is fully functional. Real serial AT communication is a placeholder ready for implementation when hardware is available.

---

## 📂 Repository Architecture (The Template Pattern)

This package is split into two distinct layers to enforce separation of concerns:

```
at_modem_monitor/
├── include/
│   └── at_modem_monitor/
│       └── at_modem_monitor_base.hpp     ← 🧠 THE REUSABLE ENGINE
├── src/
│   └── at_modem_monitor_node.cpp         ← 🚁 PROJECT-SPECIFIC PLUGIN
├── CMakeLists.txt
└── package.xml
```

| File | Role | Modify when... |
| :--- | :--- | :--- |
| `.hpp` (Header) | Contains the class `AtModemMonitorBase` with all publishers, timer, mock logic, and virtual hooks. | **Never.** This is the locked engine. |
| `.cpp` (Source) | Inherits the base class and overrides hooks for mock data or real serial implementation. | **Adapting** mock values or implementing real AT serial communication. |

---

## ✨ Key Features & Academic Requirements

This package was explicitly designed to be a **reusable template** for any robotics project:

* **🧬 Virtual Hook Polymorphism:** Override `generate_mock_data()` and `read_real_at_data()` to adapt to any modem or test scenario without rewriting the core logic. This satisfies the **Open/Closed Principle** — open for extension, closed for modification.
* **🎭 Built-in Mock Mode:** Run with `mock_mode:=true` (default) to develop dashboards and test supervisors without physical hardware. Switch to `mock_mode:=false` when a real modem is connected.
* **⏱️ Configurable via ROS Parameters:** `mock_mode` and `serial_port` can be set from the command line without recompiling.
* **🛡️ Hard DDS QoS Enforcement:** The heartbeat publisher uses `deadline` (2.5s) and `liveliness` (5s lease) QoS to enable supervisors to detect node failure the exact millisecond it happens.
* **🚫 Zero Custom Dependencies:** No custom messages, no custom services — only standard `std_msgs`.

---

## 🚀 Quick Start

### 1. Build the Workspace
Open a terminal in your ROS 2 workspace root (e.g., `~/ros2_ws`):
```bash
colcon build --packages-select at_modem_monitor
source install/setup.bash
```

### 2. Run in Mock Mode (Default — No Hardware Needed)
```bash
ros2 run at_modem_monitor at_modem_monitor_node
```

### 3. Run with Real Serial Port (When Hardware is Available)
```bash
ros2 run at_modem_monitor at_modem_monitor_node --ros-args \
  -p mock_mode:=false \
  -p serial_port:=/dev/ttyUSB0
```

---

## ⚙️ Configuration Guide

### ROS Parameters (Command-Line Configurable)

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `mock_mode` | `bool` | `true` | Use mock data instead of real serial |
| `serial_port` | `string` | `/dev/ttyUSB0` | Serial port path for real modem |

### Available Virtual Hooks

| Hook | Purpose | Default Behaviour |
| :--- | :--- | :--- |
| `generate_mock_data()` | Returns comma-separated mock values | `"CONNECTED,MOCK_OPERATOR_AT+COPS,LTE/4G_AT^SYSINFOEX,..."` |
| `read_real_at_data()` | Reads from serial port using AT commands | Returns `"DISCONNECTED,--,--,--,--,--,--,--"` (placeholder) |
| `on_poll()` | Called after every publish cycle | No action |

### Mock Data Format

The mock data string is comma-separated with 8 fields:
```
state,operator,rat,rssi,rsrp,rsrq,sinr,plmn
```

Example:
```
CONNECTED,MOCK_OPERATOR_AT+COPS,LTE/4G_AT^SYSINFOEX,-65dBm_AT+CSQ,-96dBm_AT^HCSQ,-12dB_AT^HCSQ,2dB_AT^HCSQ,26202_AT+COPS
```

---

## 🧑‍💻 Reusability Guide (For Future Projects)

This package proves true reusability. To use the AT Modem Monitor in a completely different project (e.g., an autonomous tractor with a Quectel RM500Q modem), **you do not modify the `.hpp` engine.**

### Step 1: Include the Template Engine
```cpp
#include "at_modem_monitor/at_modem_monitor_base.hpp"
```

### Step 2: Create a New Implementation Plug-in
```cpp
class TractorAtMonitor : public at_modem_monitor::AtModemMonitorBase
{
public:
  TractorAtMonitor() : AtModemMonitorBase("tractor_at_modem") {}

protected:
  // Custom mock data for tractor testing
  std::string generate_mock_data() override
  {
    return "CONNECTED,FarmNet_AT+COPS,5G_NR_AT^SYSINFOEX,"
           "-55dBm_AT+CSQ,-85dBm_AT^HCSQ,-8dB_AT^HCSQ,"
           "12dB_AT^HCSQ,310410_AT+COPS";
  }

  // Implement real serial AT commands for Quectel modem
  std::string read_real_at_data() override
  {
    // Example: open serial port, send AT+CSQ, AT+COPS?, AT^HCSQ?
    // Parse responses and return comma-separated string
    //
    // std::string rssi = send_at_command("AT+CSQ");
    // std::string op   = send_at_command("AT+COPS?");
    // ...
    // return state + "," + op + "," + rat + "," + rssi + ...;
    
    return "DISCONNECTED,--,--,--,--,--,--,--";
  }

  // React to signal quality
  void on_poll() override
  {
    // Custom logic: e.g., switch to backup network if RSSI < -100
  }
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TractorAtMonitor>());
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
| `/network/at_lte/state` | `std_msgs/String` | `"CONNECTED"` / `"DISCONNECTED"` | Modem connection state |
| `/network/at_lte/operator` | `std_msgs/String` | `"MOCK_OPERATOR_AT+COPS"` / `"--"` | Network operator (AT+COPS?) |
| `/network/at_lte/rat` | `std_msgs/String` | `"LTE/4G_AT^SYSINFOEX"` / `"--"` | Radio Access Technology |
| `/network/at_lte/rssi_dbm` | `std_msgs/String` | `"-65dBm_AT+CSQ"` / `"--"` | Signal strength (AT+CSQ) |
| `/network/at_lte/rsrp_dbm` | `std_msgs/String` | `"-96dBm_AT^HCSQ"` / `"--"` | RSRP (AT^HCSQ?) |
| `/network/at_lte/rsrq_db` | `std_msgs/String` | `"-12dB_AT^HCSQ"` / `"--"` | RSRQ (AT^HCSQ?) |
| `/network/at_lte/sinr_db` | `std_msgs/String` | `"2dB_AT^HCSQ"` / `"--"` | SINR (AT^HCSQ?) |
| `/network/at_lte/plmn` | `std_msgs/String` | `"26202_AT+COPS"` / `"--"` | PLMN code (AT+COPS?) |
| `/network/at_lte/heartbeat` | `std_msgs/String` | `"at_modem_monitor_node alive"` | Liveliness heartbeat |

### Heartbeat QoS
| Setting | Value |
| :--- | :--- |
| **Reliability** | Reliable |
| **Deadline** | 2500 ms |
| **Liveliness** | Manual by Topic |
| **Lease Duration** | 5000 ms |

---

## ⚠️ AT Command Stability Notes

The professor identified that polling AT commands **too frequently** can cause modem instability. Key recommendations:

| Issue | Solution |
| :--- | :--- |
| Connection errors after repeated polling | Increase poll period (default is 2000ms) |
| Modem stops responding | Add delays between individual AT commands |
| USB disconnects under load | Use powered USB hub, reduce poll frequency |

When implementing `read_real_at_data()`, consider adding **2-second delays** between individual AT command requests as suggested by the professor.

---

## 🎓 Design Patterns Used (Academic Reference)

| Pattern | Implementation | Benefit |
| :--- | :--- | :--- |
| **Template Method** | `AtModemMonitorBase` with virtual hooks | Reusable skeleton, project-specific filling |
| **Strategy Pattern** | `generate_mock_data()` vs `read_real_at_data()` | Swap between mock and real without class change |
| **Observer Pattern** | DDS liveliness callbacks via `assert_liveliness()` | Reactive failure detection |

---

## 📄 License
MIT License. Free to use for academic and commercial projects.
