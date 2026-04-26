from setuptools import setup, find_packages

package_name = 'rumble_teleop'

setup(
    name=package_name,
    version='1.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        # Ament index resource — required for ROS 2 package discovery
        ('share/ament_index/resource_index/packages',
         ['resource/' + package_name]),
        # package.xml
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='PathFinders',
    maintainer_email='pathfinders@example.com',
    description=(
        'ROS 2 teleoperation node for TurtleBot3 using the rumble '
        'Xbox 1708 character device driver.'
    ),
    license='GPL-2.0-only',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            # ros2 run rumble_teleop rumble_teleop_node
            'rumble_teleop_node = rumble_teleop.rumble_teleop_node:main',
        ],
    },
)
