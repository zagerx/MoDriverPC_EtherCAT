cmake_minimum_required(VERSION 3.16)
project(ecat_cli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(mo_ecat_core REQUIRED)

add_executable(ecat_cli
    main.cpp
    ecat_application.cpp
    stdin_command_reader.cpp
)

target_link_libraries(ecat_cli PRIVATE mo_ecat_core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ecat_cli PRIVATE
        -Wall
        -Wextra
    )
endif()
