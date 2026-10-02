cmake_minimum_required(VERSION 3.25)

project(quintum_core
    VERSION 0.0.1
    DESCRIPTION "QUINTUM Core"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(quintumd
    src/main.cpp
)

target_compile_features(quintumd PRIVATE cxx_std_23)

if(MSVC)
    target_compile_options(quintumd PRIVATE /W4 /permissive-)
else()
    target_compile_options(quintumd PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
