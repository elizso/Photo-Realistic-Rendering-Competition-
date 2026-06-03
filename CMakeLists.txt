cmake_minimum_required(VERSION 3.10)
project(RayTracing)

## Enable C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
# set(OpenMP_CXX_LIB_NAMES "omp")
# set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")

# include_directories("/opt/homebrew/opt/libomp/include")
# link_directories("/opt/homebrew/opt/libomp/lib")

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

set(IMGUI_DIR external/imgui)

include_directories(${IMGUI_DIR})
include_directories(${IMGUI_DIR}/backends)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(PROJECT_SOURCES
    main.cpp
    Object.hpp
    Vector.cpp
    Vector.hpp
    Sphere.hpp
    global.hpp
    Triangle.hpp
    Scene.cpp
    Scene.hpp
    BVH.cpp
    BVH.hpp
    Bounds3.hpp
    Ray.hpp
    Material.hpp
    Intersection.hpp
    Renderer.cpp
    Renderer.hpp
    SpectralSample.hpp
)

# OpenMP
find_package(OpenMP COMPONENTS CXX)
if(OpenMP_CXX_FOUND)

    add_executable(RayTracing
        ${PROJECT_SOURCES}
        ${IMGUI_SOURCES}
    )

    target_link_libraries(RayTracing PUBLIC
        OpenMP::OpenMP_CXX
        ${OpenCV_LIBS}
        glfw
        OpenGL::GL
    )

    message(STATUS "OpenMP found. Compiling with OpenMP support.")

else()
    add_executable(RayTracing
        ${PROJECT_SOURCES}
        ${IMGUI_SOURCES}
    )

    target_link_libraries(RayTracing PUBLIC
        ${OpenCV_LIBS}
        glfw
        OpenGL::GL
    )

    message(STATUS "OpenMP not found. Compiling without OpenMP support.")

endif()

if(APPLE AND NOT OpenMP_CXX_FOUND)
    target_include_directories(RayTracing PUBLIC /opt/homebrew/opt/libomp/include)
    target_link_directories(RayTracing PUBLIC /opt/homebrew/opt/libomp/lib)
    target_link_libraries(RayTracing PUBLIC omp)
    target_compile_options(RayTracing PUBLIC -Xpreprocessor -fopenmp)
    message(STATUS "Using Apple Silicon Homebrew libomp fallback.")
endif()