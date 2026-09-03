# CMakeLists.txt for GHOST C API

# Minimum required CMake version
cmake_minimum_required(VERSION 3.15)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the C API library target
add_library(ghost_c SHARED
    src/ghost_c_api.cpp
)

target_compile_definitions(ghost_c PRIVATE GHOST_C_API_BUILD)

# Include directories
target_include_directories(ghost_c
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Build a self-contained C ABI library. Position-independent code is required
# when the static GHOST core is linked into a shared library on Unix. Hide the
# C++ implementation symbols so consumers only depend on the exported C ABI.
set_target_properties(ghost_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
set_target_properties(ghost_c PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
target_link_libraries(ghost_c PRIVATE ghost_static)

# Installation rules
install(TARGETS ghost_c
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES include/ghost_c_api.h
    DESTINATION include
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
