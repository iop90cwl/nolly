cmake_minimum_required(VERSION 3.14)

project(molly VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Common compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic -Wformat=2 -Wshadow)

# Shared device + config sources
set(COMMON_SOURCES device.cc config.cc)

add_executable(molly-test molly-test.cc device.cc)
target_compile_options(molly-test PRIVATE -Wall -Wextra)

add_executable(mollyd molly-daemon.cc ${COMMON_SOURCES})
target_compile_options(mollyd PRIVATE -Wall -Wextra)

install(TARGETS mollyd molly-test DESTINATION bin)
install(FILES mollyd.conf.example DESTINATION /etc RENAME mollyd.conf OPTIONAL)
