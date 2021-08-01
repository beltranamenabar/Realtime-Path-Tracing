cmake_minimum_required(VERSION 3.10)

# Set the project name
project("OpenCL raytracing" VERSION 1.0)


# Add libraries

# Add GLUT
set(FREEGLUT_BUILD_SHARED_LIBS OFF CACHE BOOL "Build FreeGLUT shared library.")
set(FREEGLUT_BUILD_DEMOS OFF CACHE BOOL "Build FreeGLUT demos.")
add_subdirectory(extern/freeglut-3.2.1)

# Add OpenCL headers
set(BUILD_DOCS OFF CACHE BOOL "Build Documentation")
set(BUILD_EXAMPLES OFF CACHE BOOL "Build Examples")
set(BUILD_TESTS OFF CACHE BOOL "Build Unit Tests")
add_subdirectory(extern/OpenCL-CLHPP)

# Add Boost thread
find_package(Boost QUIET REQUIRED COMPONENTS thread)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost Not found")
endif()


# Add the executable
add_executable(raytrace
    src/raytrace.cpp
    src/raytrace.hpp
    src/geom.h
    src/geomfunc.h
    src/vec.h
)

# Add libraries to the executable
target_link_libraries(raytrace
    PUBLIC OpenCL
    PUBLIC freeglut_static
    PUBLIC Boost::thread
)

target_include_directories(raytrace
    PUBLIC ${OPENCL_INCLUDE_DIR}
    PUBLIC ${Boost_INCLUDE_DIR}
)

add_custom_command(
    TARGET raytrace POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res ${CMAKE_CURRENT_BINARY_DIR}/res
    COMMENT "Copying res" VERBATIM
)