add_library(liba)

target_sources(liba 
  PRIVATE
    source/src1.cc
    source/src2.cc
)

target_include_directories(liba 
  PUBLIC
     include
  PRIVATE
    include/liba
    include/liba/detail
)

add_library(ia::liba ALIAS liba) 
