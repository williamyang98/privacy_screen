cmake_minimum_required(VERSION 3.10)
project(privacy_screen)

set(ARGPARSE_INSTALL OFF CACHE BOOL "Include an install target" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/argparse)

# for posix threads
if(NOT WIN32) 
    find_package(Threads REQUIRED)
    add_compile_options(-pthread) 
    link_libraries(Threads::Threads)
endif()
find_package(GLEW CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# private compiler flags from CMakePresets.json
function(add_project_target_flags target)
    separate_arguments(PRIVATE_ARGS NATIVE_COMMAND "${PROJECT_TARGET_PRIVATE_COMPILER_FLAGS}")
    target_compile_options(${target} PRIVATE ${PRIVATE_ARGS})
endfunction()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SRC_FILES 
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/gl_assert.cpp
    ${SRC_DIR}/key_listener.cpp
)
add_executable(privacy_screen ${SRC_FILES})
target_compile_features(privacy_screen PRIVATE cxx_std_17)
add_project_target_flags(privacy_screen)
target_link_libraries(privacy_screen PUBLIC glfw GLEW::GLEW)
target_include_directories(privacy_screen PRIVATE ${SRC_DIR} ${CMAKE_SOURCE_DIR}/vendor/stb)
if(WIN32)
    target_link_libraries(privacy_screen PUBLIC OpenGL::GL)
else()
    target_link_libraries(privacy_screen PUBLIC ${OPENGL_LIBRARIES} dl)
endif()