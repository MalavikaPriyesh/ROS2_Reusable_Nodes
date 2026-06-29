cmake_minimum_required(VERSION 3.8)
project(supervisor_pkg)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(drone_health_interfaces REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# Generate custom SupervisorStatus message
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/SupervisorStatus.msg"
  DEPENDENCIES std_msgs
)

# Export the Reusable Header Library
add_library(supervisor_lib INTERFACE)
target_include_directories(supervisor_lib INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(supervisor_lib INTERFACE
  rclcpp
  drone_health_interfaces
  std_msgs
  std_srvs
)

# Build the Node Executable
add_executable(supervisor_node src/supervisor_node_main.cpp)
target_link_libraries(supervisor_node supervisor_lib)
ament_target_dependencies(supervisor_node
  rclcpp
  std_msgs
  std_srvs
  drone_health_interfaces
)

# Link the generated message library
rosidl_target_interfaces(supervisor_node
  ${PROJECT_NAME} "rosidl_typesupport_cpp"
)

install(TARGETS supervisor_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config/ launch/ DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_dependencies(rclcpp drone_health_interfaces std_msgs std_srvs rosidl_default_runtime)

ament_package()
