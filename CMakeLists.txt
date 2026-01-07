# CMake project for or testing side effecting of pending debug exceptions in
# KVM guest VMs. Include source and define project specific logic here.
#
cmake_minimum_required(VERSION 3.10)
project(debug_test C)

if(WIN32)
	set(SOURCES "main_win.c")
else()
	set(SOURCES "main.c")
endif()

add_executable(debug_test ${SOURCES})
