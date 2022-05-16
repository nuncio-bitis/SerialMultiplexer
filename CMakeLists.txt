cmake_minimum_required(VERSION 3.10)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# set the project name and version
project(SERIAL_MUX VERSION 1.0)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

configure_file(serial-mux.h.in ${PROJECT_SOURCE_DIR}/serial-mux.h)

add_executable(serial-mux serial-mux.cpp serial-mux.h)

target_link_libraries(serial-mux PUBLIC pthread)

add_subdirectory(test)

install(TARGETS serial-mux DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" )
