cmake_minimum_required(VERSION 3.24)

set(target_name blend2)

set(hdr
	BlendNode2.h
)

set(src
	main.cpp
	BlendNode2.cpp
)

set(libs
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
	target_link_libraries(${target_name} debug ${${lib}_DEBUG} optimized ${${lib}_RELEASE})
endforeach()

set_target_properties(${target_name} PROPERTIES FOLDER "osg/sample")

set(SHADERS
	blend.vert
	blend.frag

	meshkin1.frag
	meshkin2.frag

	bavoil1.frag
	bavoil2.frag
)

source_group(shaders FILES ${SHADERS})
target_sources(${target_name} PRIVATE ${SHADERS})

