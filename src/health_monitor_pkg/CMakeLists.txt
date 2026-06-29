cmake_minimum_required(VERSION 3.8)
project(health_monitor_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(drone_health_interfaces REQUIRED)

# Export the Reusable Template Header
add_library(health_monitor_lib INTERFACE)
target_include_directories(health_monitor_lib INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(health_monitor_lib INTERFACE
  rclcpp
  drone_health_interfaces
)

# Build the Drone-specific executable
add_executable(health_monitor_node src/health_monitor_node.cpp)
target_link_libraries(health_monitor_node health_monitor_lib)
ament_target_dependencies(health_monitor_node
  rclcpp
  std_msgs
  sensor_msgs
  geometry_msgs
  drone_health_interfaces
)

install(TARGETS health_monitor_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config/ launch/ DESTINATION share/${PROJECT_NAME} OPTIONAL)

ament_export_include_directories(include)
ament_export_dependencies(rclcpp drone_health_interfaces)

ament_package()
