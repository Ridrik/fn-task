cmake_minimum_required(VERSION 3.21...4.1)

project(task VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(task INTERFACE)
add_library(fn::task ALIAS task)

target_compile_features(task INTERFACE cxx_std_23)
target_sources(task INTERFACE 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES include/task/task.hpp
)

target_include_directories(task INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


option(TASK_INSTALL "Generate install rules for task" ${PROJECT_IS_TOP_LEVEL})
option(TASK_TESTS "Generate test targets and related external modules" ${PROJECT_IS_TOP_LEVEL})

if(TASK_INSTALL)
    install(TARGETS task
        EXPORT TaskTargets
        FILE_SET HEADERS
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/taskConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(EXPORT TaskTargets
        FILE TaskTargets.cmake
        NAMESPACE fn::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/task
    )
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/taskConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/taskConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/task
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/taskConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/taskConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/task
    )
endif()

if(TASK_TESTS)
    add_subdirectory(tests)
endif()