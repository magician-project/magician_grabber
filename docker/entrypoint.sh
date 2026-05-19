#!/bin/bash
# Container entrypoint: source ROS, build the magician_grabber package.
set -e

source /opt/ros/rolling/setup.bash

ROS_WS="/home/${USER}/ros_ws"
WORKSPACE="/home/${USER}/workspace"

# Build the rclcpp_magician_grabber ROS package if not already built.
# The package lives in the volume-mounted workspace.
if [ ! -f "${ROS_WS}/install/setup.bash" ]; then
    echo "[entrypoint] Building ROS workspace..."
    mkdir -p "${ROS_WS}/src"
    ln -sfn "${WORKSPACE}" "${ROS_WS}/src/rclcpp_magician_grabber"
    cd "${ROS_WS}"
    colcon build --symlink-install
    echo "[entrypoint] Build complete."
fi

source "${ROS_WS}/install/setup.bash"

exec "$@"
