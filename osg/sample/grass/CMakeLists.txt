cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(target_name grass)

set(hdr
	GrassNode.h
)

set(src
	main.cpp
	GrassNode.cpp
)

set(libs
	OPENTHREADS_LIBRARY
	OSG_LIBRARY
	OSGDB_LIBRARY
	OSGUTIL_LIBRARY
	OSGVIEWER_LIBRARY
	OSGGA_LIBRARY
)

add_executable(${target_name} ${src} ${hdr})

set_target_properties(${target_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target_name}>"
                                               VS_DEBUGGER_COMMAND           "$<TARGET_FILE:${target_name}>"
                                               VS_DEBUGGER_ENVIRONMENT       "PATH=%PATH%;${runtime_dir}")

target_include_directories(${target_name} PRIVATE ${OSG_INCLUDE_DIR})

foreach(lib ${libs})
	#target_link_libraries(${target_name} debug ${${lib}_DEBUG} optimized ${${lib}_RELEASE})
	target_link_libraries(${target_name} PRIVATE ${${lib}})
endforeach()

set_target_properties(${target_name} PROPERTIES FOLDER "osg/sample")
