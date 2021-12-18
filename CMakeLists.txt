cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(PROJECT_NAME "tinycplus")
set(TINY_LIBRARIES "")

project(${PROJECT_NAME})

include_directories(${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.h")

add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} ${TINY_LIBRARIES})
