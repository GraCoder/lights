cmake_minimum_required(VERSION 3.24)

cmake_policy(SET CMP0091 OLD)

project(lights)

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_C_FLAGS_DEBUG "/Zi /Ob0 /Od /MD")
set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Ob0 /Od /MD")

set(CMAKE_CXX_STANDARD 20)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>")

option(build_opengl "" ON)
option(build_vulkan "" ON)

configure_file(config config.h)

include_directories(${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/inc)

set(third_dir "" CACHE PATH "")

if(build_opengl)
  add_subdirectory(osg)
endif()

if(build_vulkan)
  add_subdirectory(vulkan)
endif()

add_subdirectory(test)
