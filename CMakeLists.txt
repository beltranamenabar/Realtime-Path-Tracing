cmake_minimum_required(VERSION 3.12)

# Set the project name
project("OpenCL raytracing" VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Add libraries

# Add GLUT
set(FREEGLUT_BUILD_SHARED_LIBS OFF CACHE BOOL "Build FreeGLUT shared library.")
set(FREEGLUT_BUILD_DEMOS OFF CACHE BOOL "Build FreeGLUT demos.")
add_subdirectory(extern/freeglut-3.2.1)

# Add OpenCL headers
find_package(OpenCL REQUIRED)

# Add Boost thread
find_package(Boost 1.76.0 REQUIRED COMPONENTS thread)

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
    PUBLIC OpenCL::OpenCL
    PUBLIC freeglut_static
    PUBLIC Boost::thread
)

target_include_directories(raytrace
    PUBLIC ${Boost_INCLUDE_DIR}
)

add_custom_command(
    TARGET raytrace POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res ${CMAKE_CURRENT_BINARY_DIR}/res
    COMMENT "Copying res" VERBATIM
)