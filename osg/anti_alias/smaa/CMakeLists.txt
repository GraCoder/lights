cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(target_name smaa)

set(hdr
	testNode.h
)

set(src
	main.cpp
	testNode.cpp
)

set(libs
	OSG_LIBRARY
	OSGDB_LIBRARY
	OSGUTIL_LIBRARY
	OSGVIEWER_LIBRARY
	OSGGA_LIBRARY
)

add_executable(${target_name} ${src} ${hdr})

target_compile_definitions(${target_name} PRIVATE RS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(${target_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target_name}>"
                                               VS_DEBUGGER_COMMAND           "$<TARGET_FILE:${target_name}>"
                                               VS_DEBUGGER_ENVIRONMENT       "PATH=%PATH%;${runtime_dir}")

target_include_directories(${target_name} PRIVATE ${OSG_INCLUDE_DIR})
#target_include_directories(${target_name} PRIVATE ${Boost_INCLUDE_DIR})

foreach(lib ${libs})
	target_link_libraries(${target_name} debug ${${lib}_DEBUG} optimized ${${lib}_RELEASE})
endforeach()

set_target_properties(${target_name} PROPERTIES FOLDER "osg/anti_alias")

#set(SHADERS
#	../glsl/noise.glsl
#)
#
#source_group(shaders FILES ${SHADERS})
#target_sources(${target_name} PRIVATE ${SHADERS})

