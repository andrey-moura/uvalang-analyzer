#Require a minimum version
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

project(uvalang-analyzer)

include("../uva-lang/CMakeLists.txt")

uva_library(string)

include_directories(${CMAKE_CURRENT_LIST_DIR})

add_executable(uvalang-analyzer
    ${CMAKE_CURRENT_LIST_DIR}/src/analyzer.cpp
)

target_link_libraries(uvalang-analyzer uva-extension uva-preprocessor uva-interpreter uva-lang uva-core uva-console uva-file uva-string ${wxWidgets_LIBRARIES})

install(TARGETS uvalang-analyzer DESTINATION bin)