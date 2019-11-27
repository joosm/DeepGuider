cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(BINDIR	"${CMAKE_SOURCE_DIR}/../../bin")
set(AUTHOR_NAME	"jaeyeong.lee")

get_filename_component(ProjectId ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})
project(${ProjectId} C CXX)

find_package( PythonInterp 3.6 REQUIRED )
find_package( PythonLibs 3.6 REQUIRED )
find_package( OpenCV 4.0 REQUIRED )

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR}/../../src ${PYTHON_INCLUDE_DIRS} )

file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/../../src/*.cpp ${CMAKE_SOURCE_DIR}/../../src/core/*.cpp ${CMAKE_SOURCE_DIR}/../../src/poi_recog/*.cpp *.cpp)
 
add_executable(${PROJECT_NAME} ${SOURCES}  )
target_link_libraries( ${PROJECT_NAME} ${OpenCV_LIBS} ${PYTHON_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION ${BINDIR})