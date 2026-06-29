cmake_minimum_required(VERSION 3.8)
project(management_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(drone_health_interfaces REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# Generate custom message and service interfaces
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/ManagementState.msg"
  "srv/SetTopicInactive.srv"
  "srv/SetModuleInactive.srv"
  DEPENDENCIES std_msgs
)

# Export the Reusable Template Header Library
add_library(management_lib INTERFACE)
target_include_directories(management_lib INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(management_lib INTERFACE
  rclcpp
  std_srvs
  drone_health_interfaces
)

# Build the Drone-Specific Executable
add_executable(management_node src/management_node.cpp)
target_link_libraries(management_node management_lib)
ament_target_dependencies(management_node
  rclcpp
  std_msgs
  std_srvs
  drone_health_interfaces
)

# Link the generated interface types to the executable
rosidl_target_interfaces(management_node
  ${PROJECT_NAME} "rosidl_typesupport_cpp"
)

install(TARGETS management_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config/ launch/ DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_dependencies(rclcpp drone_health_interfaces rosidl_default_runtime)

ament_package()
