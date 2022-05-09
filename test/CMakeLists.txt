cmake_minimum_required(VERSION 3.10)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(sertest sertest.cpp)

target_include_directories(sertest PUBLIC ${PROJECT_SOURCE_DIR}/../Tools/include /opt/homebrew/include)

target_link_libraries(sertest PUBLIC ${PROJECT_SOURCE_DIR}/../Tools/bin/libcpp_tools.a)

install(TARGETS sertest DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" )
