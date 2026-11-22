cmake_minimum_required(VERSION 3.22)
project(alice LANGUAGES CXX)

add_library(alice INTERFACE)
target_compile_features(alice INTERFACE cxx_std_20)
target_include_directories(alice INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
  target_compile_options(alice INTERFACE /W4 /permissive-)
else()
  target_compile_options(alice INTERFACE -Wall -Wextra -Wpedantic
                                          -Wconversion -Wsign-conversion)
endif()

option(ALICE_BUILD_TEST "Build unit tests" ON)
if(ALICE_BUILD_TEST)
  enable_testing()
  add_executable(test_memory test/test_memory.cpp)
  target_link_libraries(test_memory PRIVATE alice)
  add_test(NAME memory_smoke COMMAND test_memory)
endif()
