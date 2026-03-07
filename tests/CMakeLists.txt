
add_library(aggregator OBJECT aggregator.cpp)
target_link_libraries(aggregator PRIVATE fn::task)
set_target_properties(aggregator PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_executable(sample sample.cpp)
target_link_libraries(sample PRIVATE fn::task)
set_target_properties(sample PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_executable(sample_named_task sample_named_task.cpp)
target_link_libraries(sample_named_task PRIVATE fn::task)
set_target_properties(sample_named_task PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_subdirectory(extern)

add_executable(task_nanobench task_nanobench.cpp)
target_link_libraries(task_nanobench PRIVATE nanobench fn::task fault::fault)
set_target_properties(task_nanobench PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(WIN32)
    add_custom_command(TARGET task_nanobench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:fault::fault>
        $<TARGET_FILE_DIR:task_nanobench>
    )
    add_custom_command(TARGET task_nanobench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:cpptrace::cpptrace>
        $<TARGET_FILE_DIR:task_nanobench>
    )
endif()