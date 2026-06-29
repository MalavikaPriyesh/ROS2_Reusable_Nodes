import os
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('health_monitor_pkg'),
        'config',
        'health_monitor_params.yaml'
    )
    return LaunchDescription([
        Node(
            package='health_monitor_pkg',
            executable='health_monitor_node',
            name='health_monitor_node',
            output='screen',
            parameters=[config]
        )
    ])
