cmake_minimum_required(VERSION 3.25)

project(quintum_core
    VERSION 0.0.1
    DESCRIPTION "QUINTUM Core"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(quintum_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()

add_executable(quintumd
    src/main.cpp
)
target_compile_features(quintumd PRIVATE cxx_std_23)
target_include_directories(quintumd PRIVATE src)
quintum_strict_warnings(quintumd)

enable_testing()

add_executable(quintum_tests
    tests/foundation_tests.cpp
)
target_compile_features(quintum_tests PRIVATE cxx_std_23)
target_include_directories(quintum_tests PRIVATE src)
quintum_strict_warnings(quintum_tests)

add_test(NAME foundation COMMAND quintum_tests)
