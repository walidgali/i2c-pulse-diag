cmake_minimum_required(VERSION 3.15)
project(i2c-pulse-diag C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(i2c_core STATIC i2c_linux.c)
target_include_directories(i2c_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

option(I2C_BUILD_MOCK "Build mock I2C backend" ON)
if(I2C_BUILD_MOCK)
    add_library(i2c_mock STATIC i2c_mock.c)
    target_link_libraries(i2c_mock PUBLIC i2c_core)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.c")
    add_executable(i2c-pulse-diag main.c)
    target_link_libraries(i2c-pulse-diag PRIVATE i2c_core)
    include(GNUInstallDirs)
    install(TARGETS i2c-pulse-diag RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
