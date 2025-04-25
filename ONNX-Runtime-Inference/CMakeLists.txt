cmake_minimum_required(VERSION 3.13)

project(ONNX_Runtime_Examples VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(OpenCV REQUIRED HINTS /usr/lib/x86_64-linux-gnu/cmake/opencv4/)
find_path(ONNX_RUNTIME_SESSION_INCLUDE_DIRS onnxruntime_cxx_api.h HINTS ~/BTP/onnxruntime-linux-x64-1.12.0/include)

find_library(ONNX_RUNTIME_LIB onnxruntime HINTS ~/BTP/onnxruntime-linux-x64-1.12.0/lib)

add_subdirectory(src)