cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(SERIAL_MUX VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

configure_file(serial-mux.h.in ${PROJECT_SOURCE_DIR}/serial-mux.h)

add_executable(serial-mux serial-mux.cpp)

