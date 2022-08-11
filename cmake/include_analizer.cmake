cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
# Collect all currently added targets in all subdirectories
#
# Parameters:
# - result the list containing all found targets
# - dir root directory to start looking from
function(get_all_targets result dir)
    get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_subdir IN LISTS subdirs)
        get_all_targets(${result} "${_subdir}")
    endforeach()

    get_directory_property(sub_targets DIRECTORY "${dir}" BUILDSYSTEM_TARGETS)
    set(${result} ${${result}} ${sub_targets} PARENT_SCOPE)
endfunction()

function(string_starts_with result str search)
  string(FIND "${str}" "${search}" out )
  set( ${result} ${out} PARENT_SCOPE )
endfunction()

function(scan_includes_on_targets source_root_directory output_file_name)
  set(ANALIZER_TARGETS_DATA "")
  get_all_targets(ALL_TARGETS ${source_root_directory} )
  foreach(target_name ${ALL_TARGETS})
    get_target_property(is_imported ${target_name} IMPORTED )
    if( NOT is_imported )
      string(APPEND ANALIZER_TARGETS_DATA "\ttarget_t{\"${target_name}\"sv},\n" )
      
      get_target_property( include_directories ${target_name} INCLUDE_DIRECTORIES)
      if(include_directories)
        foreach(include_path ${include_directories})
          get_filename_component(include_path "${include_path}" ABSOLUTE)
          string(APPEND ANALIZER_INCLUDES_DATA "\ttarget_include_t{\"${target_name}\"sv,\n\t\t\"${include_path}\"sv},\n" )
        endforeach()
      endif()
      
      get_target_property( include_directories ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
      if(include_directories)
        foreach(include_path ${include_directories})
          string_starts_with(res "${include_path}" "$\<BUILD_INTERFACE:")
          if( res EQUAL 0)
            string(LENGTH "${include_path}" include_path_length )
            math(EXPR value "${include_path_length} - 19" OUTPUT_FORMAT DECIMAL) 
            string(SUBSTRING "${include_path}" 18 ${value} include_path )
          endif()
          
          string_starts_with(res "${include_path}" "$\<INSTALL_INTERFACE:")
          if( NOT res EQUAL 0)
            get_filename_component(include_path "${include_path}" ABSOLUTE)
            string(APPEND ANALIZER_INTERFACE_INCLUDES_DATA "\ttarget_include_t{\"${target_name}\"sv,\n\t\t\"${include_path}\"sv},\n" )
          endif()
        endforeach()
      endif()
      
      get_target_property( link_targets ${target_name} LINK_LIBRARIES)
      if(link_targets)
        foreach(link_target ${link_targets})
          get_target_property(dep_is_imported ${link_target} IMPORTED )
          if( NOT dep_is_imported )
            get_target_property(_aliased ${link_target} ALIASED_TARGET)
            if(_aliased)
              set(link_target ${_aliased})
            endif()
            string(APPEND ANALIZER_LINK_TARGETS_DATA "\ttarget_dependency_t{\"${target_name}\"sv, \"${link_target}\"sv},\n" )
          endif()
        endforeach()
      endif()
      
      get_target_property( source_files ${target_name} SOURCES )
      if(source_files)
        get_target_property(target_source_dir ${target_name} SOURCE_DIR)
        foreach(source_file ${source_files})
          #get_filename_component(source_file "${source_file}" BASE_DIR ${target_source_dir} ABSOLUTE)
          string(APPEND ANALIZER_SOURCES_DATA "\tsource_file_t{\"${target_name}\"sv,\n\t\t\"${target_source_dir}/${source_file}\"sv},\n" )
        endforeach()
      endif()
    endif()
  endforeach()
  configure_file(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include_analizer_tmpl.cc ${output_file_name} NEWLINE_STYLE UNIX)
endfunction()