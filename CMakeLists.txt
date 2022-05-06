cmake_minimum_required(VERSION 3.5)

project(pledge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(pledge main.cpp future.hpp executor.hpp thread_pool_executor.hpp
                      promise.hpp manual_executor.hpp)
target_link_libraries(pledge PRIVATE Threads::Threads)
