cmake_minimum_required(VERSION 3.25)

set(EXPORT_COMPILE_COMMANDS ON)
project(cpp11-threadpool)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_library(threadpool)
target_sources(threadpool PRIVATE
    src/threadpool.cpp
    src/threadpool_p.cpp
    src/threadpoolthread.cpp
)

include(GoogleTest)
find_package(Threads REQUIRED)
find_package(GTest)

target_include_directories(threadpool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(threadpool PROPERTIES CXX_STANDARD 11 CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
target_link_libraries(threadpool PRIVATE Threads::Threads)

if(TARGET GTest::GTest)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
