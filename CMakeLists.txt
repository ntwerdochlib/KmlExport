cmake_minimum_required(VERSION 3.20)
project(KmlDumper)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_subdirectory(3rdparty/pugixml)
add_subdirectory(3rdparty/html-parser)
add_subdirectory(3rdparty/KZip)
add_subdirectory(3rdparty/tclap)

add_executable(${PROJECT_NAME}
	KmlDumper.h
	KmlDumper.cpp
	main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	tclap
	pugixml
	html-parser
	KZip
)

