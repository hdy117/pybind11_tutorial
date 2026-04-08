cmake_minimum_required(VERSION 3.16)
project(pybind11_trampoline_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(trampoline_example
    src/bindings.cpp
    src/describe.cpp
    src/describe_pure.cpp
)

target_include_directories(trampoline_example PRIVATE src)
