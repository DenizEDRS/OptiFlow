#!/bin/bash

# Step 1: Build the project inside the Docker container
echo "Building the C++ project inside the Docker container..."
docker run --rm \
    -v $(pwd):/workspace \
    opencv_cpp_env \
    bash -c "mkdir -p build && cd build && cmake .. && make && cd .."

echo "Build completed successfully!"
