cmake_minimum_required(VERSION 3.13)
project(whalECS CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES_BASE ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.c ${PROJECT_SOURCE_DIR}/src/*.hpp ${PROJECT_SOURCE_DIR}/src/*.h)

add_library(whalECS SHARED ${SOURCES_BASE})

target_include_directories(${PROJECT_NAME} 
    PUBLIC src
    PUBLIC "${PROJECT_SOURCE_DIR}/lib"
)

include_directories(lib)

target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:DEBUG>:${-O2}>" "-g") 
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:RELEASE>:${-O2}>")

target_compile_options(whalECS PRIVATE -fno-rtti -fno-exceptions)
target_compile_options(whalECS PRIVATE -Wall -Wextra -Wdeprecated)
target_compile_options(whalECS PRIVATE -Wno-unused-parameter)
target_compile_options(whalECS PRIVATE -fno-strict-aliasing)
target_compile_options(whalECS PRIVATE -Wno-invalid-offsetof)
