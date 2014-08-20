# 
# P6's CMakeLists file.
# 

CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT(P6)

FILE(GLOB_RECURSE P6_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

ADD_SUBDIRECTORY(cmake)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/)
SET(LIB_SUFFIX "64")
INCLUDE_DIRECTORIES(include/)
ADD_EXECUTABLE(p6 ${P6_SOURCES})

# 
# Dependencies.
#
#-------------------------------------------------------------------------------
# ...
#-------------------------------------------------------------------------------

TARGET_LINK_LIBRARIES(p6)

SET(CMAKE_CXX_FLAGS "-Wall -O3 -std=c++11")

