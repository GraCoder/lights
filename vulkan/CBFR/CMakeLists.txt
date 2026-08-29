cmake_minimum_required(VERSION 3.24)



set(target_name cbfr)

set(hdr
)

set(src
	main.cpp
)

add_executable(${target_name} ${src} ${hdr})

set(shaders
	pbr.vert
	pbr.frag
	#test.vert
	#test.frag
)

source_group(shaders FILES ${shaders})
target_sources(${target_name} PRIVATE ${shaders})


set_target_properties(${target_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                                               VS_DEBUGGER_COMMAND           "$<TARGET_FILE:${target_name}>"
                                               VS_DEBUGGER_ENVIRONMENT       "PATH=%PATH%;${runtime_dir}"
											   VS_DEBUGGER_COMMAND_ARGUMENTS "")

foreach(lib ${libs})
	target_link_libraries(${target_name} debug ${${lib}_DEBUG} optimized ${${lib}_RELEASE})
endforeach()

target_include_directories(${target_name} PRIVATE ${Vulkan_INCLUDE_DIR})
target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/vulkan/baselib)

target_link_libraries(${target_name} ${Vulkan_LIBRARY})
target_link_libraries(${target_name} baselib)


set_target_properties(${target_name} PROPERTIES FOLDER "vulkan")

compile_shader(${target_name} ENV opengl FORMAT bin SOURCES ${shaders})
