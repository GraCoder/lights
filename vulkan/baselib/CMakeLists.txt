cmake_minimum_required(VERSION 3.24)

set(target_name baselib)

set(imgui/hdr
)

set(imgui_src
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
)

set(hdr
	VulkanInstance.h
	VulkanSwapChain.h
	VulkanPass.h
	VulkanView.h
	VulkanBuffer.h
	VulkanDevice.h
	VulkanTexture.h
	VulkanImage.h
	VulkanDebug.h
	VulkanTools.h
	VulkanInitializers.hpp
	VulkanPipeline.h
	PBRPipeline.h
	TexturePipeline.h
	DepthPipeline.h
	DepthPersPipeline.h
	DepthPass.h
	HUDPipeline.h
	HUDPass.h

	HUDRect.h
	MeshInstance.h
	MeshPrimitive.h

	VulkanImGUI.h

	Manipulator.h

	GLTFLoader.h

	${imgui_hdr}
)

set(src
	VulkanInstance.cpp
	VulkanSwapChain.cpp
	VulkanView.cpp
	VulkanPass.cpp
	VulkanBuffer.cpp
	VulkanDevice.cpp
	VulkanTexture.cpp
	VulkanImage.cpp
	VulkanDebug.cpp
	VulkanTools.cpp
	VulkanPipeline.cpp

	PBRPipeline.cpp
	TexturePipeline.cpp
	DepthPipeline.cpp
	DepthPersPipeline.cpp
	DepthPass.cpp
	HUDPipeline.cpp
	HUDPass.cpp

	HUDRect.cpp
	MeshInstance.cpp
	MeshPrimitive.cpp

	VulkanImGUI.cpp

	Manipulator.cpp

	GLTFLoader.cpp

	${imgui_src}
)

source_group("imgui" FILES ${imgui_src} ${imgui_hdr})

find_package(Vulkan)

add_library(${target_name} ${src} ${hdr})


set(shaders
	shaders/pbr_clr.vert
	shaders/pbr_tex.vert
	shaders/pbr_clr.frag
	shaders/pbr_tex.frag
	shaders/depth.vert
	shaders/depth.frag
	shaders/depth_pers.vert
	shaders/depth_pers.frag
	shaders/hud.vert
	shaders/hud.frag
)

source_group(shaders FILES ${shaders})
target_sources(${target_name} PRIVATE ${shaders})

target_include_directories(${target_name} PUBLIC ${Vulkan_INCLUDE_DIR})

target_link_libraries(${target_name} PUBLIC ${Vulkan_LIBRARY})
#target_link_libraries(${target_name} PUBLIC SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(${target_name} PUBLIC SDL2 SDL2main)

set_target_properties(${target_name} PROPERTIES FOLDER "vulkan")

compile_shader(${target_name} ENV opengl FORMAT bin SOURCES ${shaders})

