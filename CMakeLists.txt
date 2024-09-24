cmake_minimum_required(VERSION 3.26)

include(FetchContent)

set(CMAKE_OSX_ARCHITECTURES arm64;x86_64)

project(diff)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME ON)

############################################################################################################################################
# Support for googletest (GTest)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        f8d7d77c06936315286eb55f8de22cd23c188571 # v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(GTEST_FORCE_SHARED_CRT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

############################################################################################################################################

set(SRC_FILES
    # test files
    ${PROJECT_SOURCE_DIR}/test/myers.cpp
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SRC_FILES})

set(HEADER_FILES
    # diff files
    ${PROJECT_SOURCE_DIR}/include/diff/myers.hpp
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${HEADER_FILES})

add_executable(difftest
    ${SRC_FILES}
    ${HEADER_FILES}
)

target_include_directories(difftest
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(difftest
    GTest::gtest_main
)

set_target_properties(difftest PROPERTIES OUTPUT_NAME "difftest")
