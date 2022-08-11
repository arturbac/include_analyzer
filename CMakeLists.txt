cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(include_analyzer
  VERSION 0.1.0
  LANGUAGES CXX
  HOMEPAGE_URL "https://github.com/arturbac/include_anaylzer"
  )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake") 

set(CMAKE_CXX_STANDARD 20 )
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF )

add_subdirectory(common)
add_subdirectory(exec)

include(include_analizer)

if(PROJECT_IS_TOP_LEVEL)
  #project development only 
  scan_includes_on_targets(${CMAKE_CURRENT_LIST_DIR} "sources.cc")
  add_executable(include_analizer "${CMAKE_CURRENT_BINARY_DIR}/sources.cc" )
endif()
