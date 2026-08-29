cmake_minimum_required(VERSION 3.24.0)

set(target_name test)


set(src
	main.cpp
)

add_executable(${target_name} ${src})

set_target_properties(${target_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target_name}>"
                                               VS_DEBUGGER_COMMAND           "$<TARGET_FILE:${target_name}>"
                                               VS_DEBUGGER_ENVIRONMENT       "PATH=%PATH%;${CMAKE_PREFIX_PATH}/bin")

