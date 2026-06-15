cmake_minimum_required(VERSION 3.16)
project(MoDriverPC_EtherCAT LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加 SOEM 子模块
add_subdirectory(third_party/SOEM)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/SOEM/soem
)

add_executable(MoDriverPC
    src/main.cpp
    src/ec_master/ec_master.cpp
    src/motor_driver/motor_driver.cpp
)

target_link_libraries(MoDriverPC soem)
