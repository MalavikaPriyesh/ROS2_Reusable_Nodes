cmake_minimum_required(VERSION 3.8)
project(safety_fusion_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(drone_health_interfaces REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# Generate the SafetyStatus message
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/SafetyStatus.msg"
  DEPENDENCIES std_msgs
)

# Export the Reusable Template Header
add_library(safety_fusion_lib INTERFACE)
target_include_directories(safety_fusion_lib INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(safety_fusion_lib INTERFACE
  rclcpp
  drone_health_interfaces
)

# Build Your Drone-Specific Node
add_executable(safety_fusion_node src/safety_fusion_node.cpp)
target_link_libraries(safety_fusion_node safety_fusion_lib)
ament_target_dependencies(safety_fusion_node
  rclcpp
  std_msgs
  geometry_msgs
  drone_health_interfaces
)

# Link the generated SafetyStatus message
rosidl_target_interfaces(safety_fusion_node
  ${PROJECT_NAME} "rosidl_typesupport_cpp"
)

install(TARGETS safety_fusion_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config/ launch/ DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_dependencies(rclcpp drone_health_interfaces rosidl_default_runtime)

ament_package()
