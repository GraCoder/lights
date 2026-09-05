cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan)
find_package(SDL3 CONFIG REQUIRED)

get_filename_component(VK_BIN_DIR ${Vulkan_GLSLC_EXECUTABLE} DIRECTORY)

set(COMMON_LIB SDL3::SDL3)

#find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)
function(compile_shader target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "ENV;FORMAT" "SOURCES")
    #set(spvs)
    foreach(source ${arg_SOURCES})
        set(out_file ${CMAKE_CURRENT_SOURCE_DIR}/${source}.spv)
        set(src_file ${CMAKE_CURRENT_SOURCE_DIR}/${source})
        add_custom_command(
            OUTPUT ${out_file}
            DEPENDS ${src_file}
            #DEPFILE ${source}.d
            COMMAND
								Vulkan::glslc
                #$<$<BOOL:${arg_ENV}>:--target-env=${arg_ENV}>
                $<$<BOOL:${arg_FORMAT}>:-mfmt=${arg_FORMAT}>
                #-MD -MF ${source}.d
                -o ${out_file} 
                ${src_file} 
        )
        target_sources(${target} PRIVATE ${source}.spv)
        list(APPEND spvs ${CMAKE_CURRENT_SOURCE_DIR}/${source}.spv)
    endforeach()
    #add_custom_target(shader_spv DEPENDS ${spvs})
    #add_dependencies(${target} shader_spv)
endfunction()

add_definitions(-D DEPTH_ZERO)

add_subdirectory(baselib)
add_subdirectory(basic_pbr)

add_subdirectory(shadow)
