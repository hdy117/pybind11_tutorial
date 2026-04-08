cmake_minimum_required(VERSION 3.16)
project(ch19_exceptions LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(exception_example
    src/bindings.cpp
)
