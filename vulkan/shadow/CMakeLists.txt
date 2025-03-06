
cmake_minimum_required(VERSION 3.24)


set(osg_build_dir)

option(build_shadowmap "build_shadowmap" ON)

if(build_shadowmap)
add_subdirectory(basic_shadowmap)
add_subdirectory(perspective_shadowmap)
endif()

