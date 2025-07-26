cmake_minimum_required(VERSION 3.10.0)
project(TimeToDX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE SOURCE_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(TimeToDX ${SOURCE_FILES})

target_link_libraries(TimeToDX
    d3d11
    dxgi
    d3dcompiler
)

target_include_directories(TimeToDX
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include
)

set_target_properties(TimeToDX PROPERTIES
    WIN32_EXECUTABLE TRUE
)
