from launch import LaunchDescription
from launch_ros.actions import Node
import os
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    config = os.path.join(get_package_share_directory('supervisor_pkg'), 'config', 'supervisor_params.yaml')
    return LaunchDescription([
        Node(package='supervisor_pkg', executable='supervisor_node', name='supervisor_node', parameters=[config], output='screen')
    ])
