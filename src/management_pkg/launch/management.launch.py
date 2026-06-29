import os
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    cfg = os.path.join(get_package_share_directory('management_pkg'), 'config', 'management_params.yaml')
    return LaunchDescription([
        Node(
            package='management_pkg',
            executable='management_node',
            name='management_node',
            output='screen',
            parameters=[cfg]
        )
    ])
