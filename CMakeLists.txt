cmake_minimum_required(VERSION 3.15)
project(deferred_decals LANGUAGES C)
set(CMAKE_C_STANDARD 99)

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
file(GLOB_RECURSE SHADERS RELATIVE ${CMAKE_SOURCE_DIR} "res/shaders/*.glsl")


add_executable(${PROJECT_NAME} ${SOURCES} ${SHADERS})
target_include_directories(${PROJECT_NAME} PRIVATE "src/" "thirdparty/")

add_subdirectory(thirdparty/glfw)
add_subdirectory(thirdparty/glad)
add_subdirectory(thirdparty/mikktspace)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw glad mikktspace)

target_compile_definitions(${PROJECT_NAME} PRIVATE RES_HOME="${CMAKE_SOURCE_DIR}/res" _CRT_SECURE_NO_WARNINGS)