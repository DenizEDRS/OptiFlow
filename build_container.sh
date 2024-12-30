#!/bin/bash

# Step 1: Build the Docker image
echo "Building the Docker image..."
docker build -t opencv_cpp_env .

echo "Docker image built successfully!"
