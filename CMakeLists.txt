# 
# P6's CMakeLists file.
# 

CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT(Arane)

FILE(GLOB_RECURSE Arane_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

ADD_SUBDIRECTORY(cmake)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/)
SET(LIB_SUFFIX "64")
INCLUDE_DIRECTORIES(include/)
ADD_EXECUTABLE(arane ${Arane_SOURCES})

# 
# Dependencies.
#
#-------------------------------------------------------------------------------
# ...
#-------------------------------------------------------------------------------

TARGET_LINK_LIBRARIES(arane)

SET(CMAKE_CXX_FLAGS "-Wall -O3 -std=c++11")

