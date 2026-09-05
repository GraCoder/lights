set(target_name soft_shadowmap)

set(hdr
	PCFShadow.h
	ShadowTechnique.h
	VSMShadow.h
	ShadowView.h
	ShadowPipeline.h
)

set(src
	main.cpp
	PCFShadow.cpp
	VSMShadow.cpp
	ShadowView.cpp
	ShadowPipeline.cpp
)

add_executable(${target_name} ${src} ${hdr})

set(shaders
	shadow.vert
	pcf_shadow.frag
	vsm_moments.vert
	vsm_moments.frag
	vsm_shadow.frag
)

source_group(shaders FILES ${shaders})
target_sources(${target_name} PRIVATE ${shaders})

set_target_properties(${target_name} PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VS_DEBUGGER_COMMAND           "$<TARGET_FILE:${target_name}>"
  VS_DEBUGGER_ENVIRONMENT       "PATH=${VK_BIN_DIR};%PATH%"
	VS_DEBUGGER_COMMAND_ARGUMENTS ""
)

target_include_directories(${target_name} PRIVATE ${Vulkan_INCLUDE_DIR})
target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/vulkan/baselib)

target_link_libraries(${target_name} ${Vulkan_LIBRARY})
target_link_libraries(${target_name} baselib)
target_link_libraries(${target_name} ${COMMON_LIB})

set_target_properties(${target_name} PROPERTIES FOLDER "vulkan/shadow")

compile_shader(${target_name} ENV opengl FORMAT bin SOURCES ${shaders})
