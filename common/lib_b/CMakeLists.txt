add_library(libb)

target_sources(libb 
  PRIVATE
    source/src1.cc
    source/src2.cc
)

target_include_directories(libb
   INTERFACE
    include
    
  PRIVATE
    include
    include/libb
)

add_library(ia::libb ALIAS libb)  
target_link_libraries( libb PUBLIC ia::liba)
