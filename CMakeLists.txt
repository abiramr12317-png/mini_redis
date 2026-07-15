cmake_minimum_required(VERSION 3.16)
project(mini_redis CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mini-redis src/main.cpp src/store.cpp)
target_include_directories(mini-redis PRIVATE include)

# Unit tests (doctest)
add_executable(run-tests tests/test_store.cpp src/store.cpp)
target_include_directories(run-tests PRIVATE include tests)

# Stress / scale test
add_executable(stress-test tests/stress_store.cpp src/store.cpp)
target_include_directories(stress-test PRIVATE include)