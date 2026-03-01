
add_executable(sample sample.cpp)
target_link_libraries(sample PRIVATE fn::task)
set_target_properties(sample PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)