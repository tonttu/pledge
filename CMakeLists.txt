cmake_minimum_required(VERSION 3.5)

project(pledge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(tests Tests.cpp Future.hpp Executor.hpp ThreadPoolExecutor.hpp
                     Promise.hpp ManualExecutor.hpp Traits.hpp)
target_link_libraries(tests PRIVATE Threads::Threads)
