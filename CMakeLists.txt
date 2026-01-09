# CMake project for or testing side effecting of pending debug exceptions in
# KVM guest VMs. Include source and define project specific logic here.
#
cmake_minimum_required(VERSION 3.10)
project(debug_test C)

set(SOURCES "main.c")

add_executable(debug_test ${SOURCES})

if(WIN32)
	target_include_directories(debug_test PRIVATE win)
else()
	target_include_directories(debug_test PRIVATE unix)
endif()
