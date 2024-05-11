cmake_minimum_required(VERSION 3.20)
project(tclap)

set(BUILD_EXAMPLES OFF)
set(BUILD_UNITESTS OFF)
set(BUILD_DOC OFF)

add_subdirectory(tclap)

add_library(${PROJECT_NAME} INTERFACE)
target_link_libraries(${PROJECT_NAME} INTERFACE TCLAP)
#target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/tclap/include)


