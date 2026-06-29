import os
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('safety_fusion_pkg'),
        'config',
        'safety_fusion_params.yaml'
    )
    return LaunchDescription([
        Node(
            package='safety_fusion_pkg',
            executable='safety_fusion_node',
            name='safety_fusion_node',
            output='screen',
            parameters=[config]
        )
    ])
