# 🎯 ROS 2 Reusable Mission Manager Node

[![ROS 2](https://img.shields.io/badge/ROS_2-Humble%20%7C%20Iron%20%7C%20Jazzy-blue)](https://docs.ros.org/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-purple.svg)](https://en.cppreference.com/w/cpp/17)

A reusable ROS 2 node that manages mission phases and communicates with the management node for mission activation.

Follows a strict **Template Engine + Implementation Plug-in** architecture.

---

## 📂 Package Structure

```text
mission_manager/
├── include/
│   └── mission_manager/
│       └── mission_manager_base.hpp   ← 🧠 THE REUSABLE ENGINE (never modify)
├── src/
│   └── mission_manager_node.cpp       ← 🚁 PROJECT-SPECIFIC PLUGIN (edit this)
├── CMakeLists.txt
└── package.xml
```

| File | Role | Modify when |
| :--- | :--- | :--- |
| `.hpp` Header | Contains `MissionConfig`, `MissionState`, and `MissionManagerBase` engine | **Never** |
| `.cpp` Source | Inherits the base and overrides hooks | **Every new project** |

---

## 🏗️ System Architecture

### State Machine

```mermaid
stateDiagram-v2
    [*] --> WAITING_TO_START : Node starts

    WAITING_TO_START --> START_REQUESTED : start_delay_s elapsed

    START_REQUESTED --> WAITING_TO_START : Management rejected
    START_REQUESTED --> INSPECTION : Management accepted

    INSPECTION --> INSPECTION_COMPLETE : inspection_duration_s elapsed

    INSPECTION_COMPLETE --> NAVIGATION_CONTINUE : post_inspection_pause_s elapsed

    NAVIGATION_CONTINUE --> [*] : Mission complete
```

### Data Flow

```mermaid
graph TD
    subgraph Config["⚙️ MissionConfig Struct"]
        C1["phase_topic"]
        C2["mission_service"]
        C3["publish_period_ms"]
        C4["start_delay_s"]
        C5["inspection_duration_s"]
        C6["post_inspection_pause_s"]
    end

    subgraph Engine["🧠 MissionManagerBase Engine"]
        TK["tick()"]
        PP["publish_phase()"]
        HS["handle_state()"]
        RMA["request_mission_active()"]
        TMR["Wall Timer"]
    end

    subgraph Hooks["🪝 Virtual Hooks"]
        H1["on_mission_started()"]
        H2["on_inspection_started()"]
        H3["on_inspection_complete()"]
        H4["on_navigation_continue()"]
        H5["on_mission_rejected()"]
        H6["on_tick()"]
    end

    subgraph Plugin["🚁 Project Plugin"]
        MY["FlyingRobotMissionManager"]
    end

    MN["management_node"]
    PT["/mission/phase"]
    DB["dashboard_gui"]

    Config --> Engine
    TMR --> TK
    TK --> PP
    TK --> HS
    HS --> RMA
    RMA --> MN
    MN --> H1
    MN --> H5
    HS --> H3
    HS --> H4
    PP --> PT
    MY -.->|overrides| H1
    MY -.->|overrides| H2
    MY -.->|overrides| H3
    MY -.->|overrides| H4
    MY -.->|overrides| H5
    PT --> DB
```

---

## 🚀 Quick Start

### Build the Package
```bash
cd ~/ros2_ws
colcon build --packages-select mission_manager
source install/setup.bash
```

### Run the Node
```bash
ros2 run mission_manager mission_manager_node
```

---

## ⚙️ Configuration Guide

All configuration is done at compile time via `MissionConfig` in the plugin constructor.

| Field | Default | Description |
| :--- | :--- | :--- |
| `phase_topic` | `/mission/phase` | Topic for publishing current phase |
| `mission_service` | `/management/set_mission_active` | Service to activate mission |
| `publish_period_ms` | `500` | How often to publish phase in ms |
| `start_delay_s` | `5` | Seconds before requesting mission |
| `inspection_duration_s` | `50` | Duration of inspection phase |
| `post_inspection_pause_s` | `2.0` | Pause after inspection before continuing |

---

## 🧑‍💻 Reusability Guide

### Step 1: Include the Engine
```cpp
#include "mission_manager/mission_manager_base.hpp"
```

### Step 2: Create Your Plugin
```cpp
class TractorMissionManager : public mission_manager::MissionManagerBase
{
public:
  TractorMissionManager()
  : MissionManagerBase("tractor_mission", build_config()) {}

protected:
  void on_inspection_started() override
  {
    // Activate soil moisture sensor
  }

  void on_inspection_complete() override
  {
    // Save soil data to file
  }

private:
  static mission_manager::MissionConfig build_config()
  {
    mission_manager::MissionConfig cfg;
    cfg.phase_topic           = "/tractor/mission/phase";
    cfg.start_delay_s         = 3;
    cfg.inspection_duration_s = 120;
    return cfg;
  }
};
```

**Zero modifications** to the original engine.

---

## 📡 Topic Interfaces

### Published Topics

| Topic | Type | Values |
| :--- | :--- | :--- |
| `/mission/phase` | `std_msgs/String` | `IDLE`, `INSPECTION`, `INSPECTION_COMPLETE`, `NAVIGATION_CONTINUE` |

### Service Clients

| Service | Type | Purpose |
| :--- | :--- | :--- |
| `/management/set_mission_active` | `std_srvs/SetBool` | Request mission activation |

---

## 🎓 Design Patterns Used

| Pattern | Implementation | Benefit |
| :--- | :--- | :--- |
| **Template Method** | `MissionManagerBase` with virtual hooks | Reusable state machine |
| **Strategy Pattern** | `phase_string()` virtual method | Custom phase naming |
| **State Pattern** | `MissionState` enum + `handle_state()` | Clean state transitions |
| **Observer Pattern** | Async service callbacks | Non-blocking activation |

---

## 📄 License

MIT License. Free to use for academic and commercial projects.
