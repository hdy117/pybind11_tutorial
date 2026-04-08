cmake_minimum_required(VERSION 3.16)
project(ch22_zero_copy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(zero_copy_example
    src/bindings.cpp
)
