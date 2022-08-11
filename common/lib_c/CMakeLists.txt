add_library(libc)

target_sources(libc 
  PRIVATE
    source/src1.cc
    source/src2.cc
)

target_include_directories(libc PUBLIC include )

add_library(ia::libc ALIAS libc)   
target_link_libraries( libc PUBLIC ia::libb ia::liba)
