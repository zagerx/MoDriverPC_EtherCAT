cmake_minimum_required(VERSION 3.16)
project(mo_ecat_core VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third_party/SOEM)

option(MO_ECAT_BUILD_TESTS "Build tests" OFF)

set(MO_ECAT_CORE_SOURCES
    src/core/mo_ecat_master.cpp
    src/core/runtime_state.cpp
    src/activity/activity_scheduler.cpp
    src/activity/pdo_mapping_activity.cpp
    src/activity/sdo_diagnostics_activity.cpp
    src/activity/sdo_parameter_activity.cpp
    src/activity/state_inspection_activity.cpp
    src/ec_master/ec_master.cpp
    src/ec_controller/ec_controller.cpp
    src/cyclic/cyclic_runner.cpp
    src/cyclic/process_data_engine.cpp
    src/servo_axis/servo_axis.cpp
    src/slave_node/device_profile.cpp
    src/slave_node/slave_config_factory.cpp
    src/slave_node/slave_node.cpp
    src/slave_node/slave_node_manager.cpp
    src/slave_node/slave_type_detector.cpp
    src/utils/logger.cpp
)

add_library(mo_ecat_core STATIC ${MO_ECAT_CORE_SOURCES})

target_include_directories(mo_ecat_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mo_ecat_core
    PUBLIC soem
    PRIVATE pthread
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mo_ecat_core PRIVATE
        -Wall
        -Wextra
    )
endif()

# Install / export rules
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS mo_ecat_core soem
    EXPORT mo_ecat_coreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT mo_ecat_coreTargets
    FILE mo_ecat_coreTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mo_ecat_core
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mo_ecat_coreConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mo_ecat_coreConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mo_ecat_core
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/mo_ecat_coreConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mo_ecat_coreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/mo_ecat_coreConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mo_ecat_core
)

# Build-tree package config, so examples/cli can use find_package without install.
export(EXPORT mo_ecat_coreTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/mo_ecat_coreTargets.cmake
)


if(MO_ECAT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
