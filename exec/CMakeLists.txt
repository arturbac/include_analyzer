add_executable(exec)

target_sources(exec 
  PRIVATE
    source/main.cc
)

target_include_directories(exec PUBLIC include )
target_link_libraries( exec PRIVATE ia::libc ia::libb)
