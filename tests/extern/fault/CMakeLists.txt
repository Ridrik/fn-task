set(FAULT_LOCATIONS "DEFAULT" CACHE STRING "" FORCE)
set(FAULT_ASSERTIONS "DEFAULT" CACHE STRING "" FORCE)
set(FAULT_BUILD_SHARED ON CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(
    fault
    GIT_REPOSITORY https://github.com/Ridrik/fault.git
    GIT_TAG v0.5.0
)
FetchContent_MakeAvailable(fault)