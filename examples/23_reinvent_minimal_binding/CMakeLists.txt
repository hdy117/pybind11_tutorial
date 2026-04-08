cmake_minimum_required(VERSION 3.16)
project(ch23_reinvent_minimal_binding LANGUAGES C)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)

add_library(raw_binding_example MODULE src/module.c)
set_target_properties(raw_binding_example PROPERTIES PREFIX "")

if(WIN32)
    set_target_properties(raw_binding_example PROPERTIES SUFFIX ".pyd")
endif()

target_include_directories(raw_binding_example PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(raw_binding_example PRIVATE Python3::Module)
