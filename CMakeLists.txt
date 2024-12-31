cmake_minimum_required(VERSION 3.10)
project(RedWindow)

# Find OpenCV
find_package(OpenCV REQUIRED)

# Add the executable
add_executable(red_window main.cpp)

# Link OpenCV libraries
target_link_libraries(red_window ${OpenCV_LIBS})
