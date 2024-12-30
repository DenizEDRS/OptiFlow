# Use a base image with C++ development tools
FROM ubuntu:20.04

# Set environment variables to avoid user interaction during installations
ENV DEBIAN_FRONTEND=noninteractive

# Update and install required packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    nano \
    libgtk-3-dev \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libv4l-dev \
    libxvidcore-dev \
    libx264-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    gfortran \
    openexr \
    python3-dev \
    python3-numpy \
    libtbb2 \
    libtbb-dev \
    libdc1394-22-dev \
    libopenexr-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-good1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    libxine2-dev \
    libvulkan-dev \
    libopencore-amrnb-dev \
    libopencore-amrwb-dev \
    libtheora-dev \
    libvorbis-dev \
    libxine2-dev \
    libgphoto2-dev \
    libeigen3-dev \
    mesa-common-dev \
    libglu1-mesa-dev \
    guvcview \
    nlohmann-json3-dev\
    && apt-get clean

# Clone OpenCV repositories
RUN git clone https://github.com/opencv/opencv.git /opencv \
    && git clone https://github.com/opencv/opencv_contrib.git /opencv_contrib

# Create build directory and configure OpenCV
RUN mkdir -p /opencv/build && cd /opencv/build && \
    cmake -D CMAKE_BUILD_TYPE=Release \
          -D CMAKE_INSTALL_PREFIX=/usr/local \
          -D OPENCV_EXTRA_MODULES_PATH=/opencv_contrib/modules \
          -D BUILD_EXAMPLES=ON \
          -D WITH_GSTREAMER=ON \
          -D WITH_V4L=ON \
          -D BUILD_TESTS=ON \
          -D ENABLE_FAST_MATH=ON \
          -D WITH_OPENGL=ON \
          -D WITH_TBB=ON \
          -D BUILD_opencv_videoio=ON \
          -D WITH_FFMPEG=ON .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig

RUN echo "alias build_main='mkdir -p build && cd build && cmake .. && make && cd ..'" >> ~/.bashrc
RUN echo "source ~/.bashrc" >> ~/.bashrc
# Set working directory
WORKDIR /workspace

# Command to run bash by default
CMD ["/bin/bash"]
