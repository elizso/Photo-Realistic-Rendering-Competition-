cmake_minimum_required(VERSION 3.10)
project(RayTracing)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

## Enable C++14
set(CMAKE_CXX_STANDARD 17)

# OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Linking the execution file
add_executable(RayTracing main.cpp Object.hpp Vector.cpp Vector.hpp Sphere.hpp global.hpp Triangle.hpp Scene.cpp
Scene.hpp BVH.cpp BVH.hpp Bounds3.hpp Ray.hpp Material.hpp Texture.hpp Intersection.hpp
Renderer.cpp Renderer.hpp)
target_link_libraries(RayTracing PRIVATE ${OpenCV_LIBRARIES})

# OpenMP
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(RayTracing PUBLIC OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP found. Compiling with OpenMP support.")
else()
    message(STATUS "OpenMP not found. Compiling without OpenMP support.")
endif()

