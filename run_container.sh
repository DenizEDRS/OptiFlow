#!/bin/bash

# Step 1: Start the Docker container with volume mounting and webcam access
echo "Starting the Docker container with webcam access..."
docker run -it --rm \
    -v "$(pwd):/workspace" \
    -e DISPLAY="$DISPLAY" \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    --device=/dev/video0:/dev/video0 \
    --privileged \
    opencv_cpp_env
