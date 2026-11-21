cmake_minimum_required(VERSION 3.15)
project(i2c-pulse-diag C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_definitions(_DEFAULT_SOURCE)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(i2c_core STATIC 
    src/i2c_linux.c 
    src/i2c_mock.c
)

enable_testing()
add_executable(test_mock tests/test_mock.c)
target_link_libraries(test_mock PRIVATE i2c_core)
add_test(NAME MockTest COMMAND test_mock)
