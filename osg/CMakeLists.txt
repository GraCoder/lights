
cmake_minimum_required(VERSION 3.24)


find_package(OpenSceneGraph COMPONENTS osg osgDB osgViewer osgUtil osgGA)

find_path(OSG_GEN_INCLUDE_PATH NAMES osg/Config osg/GL PATHS ${OSG_DIR}/include)

INCLUDE_DIRECTORIES(${OSG_GEN_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})

cmake_path(SET osg_build_dir  ${OSG_DIR})

set(runtime_dir "${osg_build_dir}/bin;${third_dir}/bin")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OSG_DIR}/bin)

add_subdirectory(basic-lighting)
add_subdirectory(basic-material)
add_subdirectory(pbr-lighting)
add_subdirectory(pbr-lighting-tex)
add_subdirectory(pbr-ibl)
add_subdirectory(pbr-ibl-lighting)
add_subdirectory(pbr-ibl-lighting-rfl)
add_subdirectory(pbr-ibl-lighting-tex)

add_subdirectory(basic)
add_subdirectory(sample)
add_subdirectory(anti_alias)
