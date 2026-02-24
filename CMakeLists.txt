cmake_minimum_required(VERSION 3.17)
project(DXEngine)
file(GLOB_RECURSE HEADERS "${CMAKE_SOURCE_DIR}/src/*.h" "${CMAKE_SOURCE_DIR}/src/*.hpp")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin")

add_executable(DXEngine)

target_include_directories(DXEngine PUBLIC ${HEADERS})

target_sources(DXEngine PUBLIC 
    "${CMAKE_SOURCE_DIR}/src/main.cpp"
    )


