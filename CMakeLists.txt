cmake_minimum_required(VERSION 3.26)

include(FetchContent)

set(CMAKE_OSX_ARCHITECTURES arm64;x86_64)

project(visibility_susser)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_GENERATOR MATCHES "Xcode")
    set(CMAKE_XCODE_GENERATE_SCHEME ON)

    # This does not appear to be set via `CMAKE_CXX_STANDARD`; not sure why.
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20")
endif()

############################################################################################################################################
# Support for googletest (GTest)
# 
# FetchContent_Declare(
#     googletest
#     GIT_REPOSITORY https://github.com/google/googletest.git
#     GIT_TAG        f8d7d77c06936315286eb55f8de22cd23c188571 # v1.14.0
# )
# # For Windows: Prevent overriding the parent project's compiler/linker settings
# set(GTEST_FORCE_SHARED_CRT ON CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(googletest)
# 
############################################################################################################################################

set(SRC_FILES
    # test files
    ${PROJECT_SOURCE_DIR}/source/main.cpp
    ${PROJECT_SOURCE_DIR}/source/shell.cpp
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SRC_FILES})

set(HEADER_FILES
    # visibility_susser files
    ${PROJECT_SOURCE_DIR}/include/vizsuss/shell.hpp
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${HEADER_FILES})

add_executable(visibility_susser
    ${SRC_FILES}
    ${HEADER_FILES}
)

target_include_directories(visibility_susser
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(visibility_susser
    # GTest::gtest_main
)

set_target_properties(visibility_susser PROPERTIES OUTPUT_NAME "visibility_susser")

if (CMAKE_GENERATOR MATCHES "Xcode")
    set_property(TARGET visibility_susser PROPERTY XCODE_SCHEME_ARGUMENTS /path/to/binary)
endif()
