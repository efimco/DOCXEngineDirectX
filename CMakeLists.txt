cmake_minimum_required(VERSION 3.10.0)
project(TimeToDX VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(TimeToDX
    src/main.cpp
    src/window.cpp
)

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
file(COPY ${CMAKE_SOURCE_DIR}/src/shaders DESTINATION ${CMAKE_BINARY_DIR}/Debug)
